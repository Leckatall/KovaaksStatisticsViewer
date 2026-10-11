#include "ui_driver.h"

#include <QAccessible>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJSEngine>
#include <QJSValue>
#include <QKeySequence>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QTest>
#include <QUrl>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>

#include "message_log.h"

namespace ksv::driver {
    namespace {
        constexpr int kDefaultSettleMs = 120;
        constexpr int kMaxCandidates = 15;
        constexpr int kMaxTreeNodes = 4000;

        // ---- object introspection ------------------------------------------------

        bool isPopup(const QObject *object) { return object && object->inherits("QQuickPopup"); }

        // A Popup is a QObject; what it draws lives in its private popup item, which
        // parents the popup's contentItem/background.
        QQuickItem *popupItemOf(const QObject *popup) {
            for (const char *property: {"contentItem", "background"}) {
                if (auto *item = popup->property(property).value<QQuickItem *>()) {
                    if (item->parentItem()) return item->parentItem();
                }
            }
            return nullptr;
        }

        bool hasProperty(const QObject *object, const char *name) {
            return object->metaObject()->indexOfProperty(name) >= 0;
        }

        QString stringProperty(const QObject *object, const char *name) {
            if (!hasProperty(object, name)) return {};
            const QVariant value = object->property(name);
            return value.typeId() == QMetaType::QString ? value.toString() : QString();
        }

        QString cleanTypeName(const QMetaObject *metaObject) {
            static const QRegularExpression qmlSuffix(QStringLiteral("(_QML(TYPE)?_\\d+)+$"));
            QString name = QString::fromLatin1(metaObject->className());
            name.remove(qmlSuffix);
            if (const auto scope = name.lastIndexOf(QStringLiteral("::")); scope >= 0) name = name.mid(scope + 2);
            if (name.startsWith(QStringLiteral("QQuick"))) name = name.mid(6);
            else if (name.startsWith(QStringLiteral("QQml"))) name = name.mid(4);
            if (name.endsWith(QStringLiteral("QmlImpl"))) name.chop(7);
            return name;
        }

        QStringList typeChain(const QObject *object) {
            QStringList chain;
            for (auto *metaObject = object->metaObject(); metaObject; metaObject = metaObject->superClass()) {
                const QString name = cleanTypeName(metaObject);
                if (!chain.contains(name)) chain.append(name);
            }
            return chain;
        }

        // Ids from Qt's own control implementations (Fusion style files) are
        // implementation detail; only ids from the app's documents are reported.
        bool isStyleContext(const QQmlContext *context) {
            return context->baseUrl().toString().contains(QStringLiteral("qt-project.org"));
        }

        QString qmlId(const QObject *object) {
            QQmlContext *context = qmlContext(object);
            for (int level = 0; context && level < 2; ++level, context = context->parentContext()) {
                if (isStyleContext(context)) continue;
                const QString name = context->nameForObject(object);
                if (name.isEmpty()) continue;
                // nameForObject() also reports properties of the context object (a
                // component's root), e.g. a control's contentItem; those aren't ids.
                if (const QObject *scope = context->contextObject();
                    scope && scope->metaObject()->indexOfProperty(name.toUtf8().constData()) >= 0) {
                    continue;
                }
                return name;
            }
            return {};
        }

        QString accessibleName(QObject *object) {
            if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(object)) {
                return iface->text(QAccessible::Name);
            }
            return {};
        }

        QString displayText(const QObject *object) {
            QString text = stringProperty(object, "text");
            if (text.isEmpty()) text = stringProperty(object, "title");
            return text;
        }

        QRectF sceneRect(const QQuickItem *item) {
            return item ? item->mapRectToScene(QRectF(0, 0, item->width(), item->height())) : QRectF();
        }

        QJsonArray rectJson(const QRectF &rect) {
            const QRect pixels = rect.toRect();
            return {pixels.x(), pixels.y(), pixels.width(), pixels.height()};
        }

        // Neither an item, a popup nor a window: an Action, Timer, Settings, a
        // QtQuick.Dialogs dialog, ...
        bool isNonVisual(const QObject *object) {
            return !qobject_cast<const QQuickItem *>(object) && !isPopup(object) &&
                   !qobject_cast<const QWindow *>(object);
        }

        bool isQtDialog(const QObject *object) { return object && object->inherits("QQuickAbstractDialog"); }

        // The QML implementation QtQuick.Dialogs shows when there is no native
        // dialog (QQuickMessageDialogImpl, QQuickFileDialogImpl, ...): a Popup.
        bool isDialogImpl(const QObject *object) {
            for (auto *metaObject = object->metaObject(); metaObject; metaObject = metaObject->superClass()) {
                if (QLatin1String(metaObject->className()).endsWith(QLatin1String("DialogImpl"))) return true;
            }
            return false;
        }

        // QML-created non-visual objects worth walking: created by a QML context
        // other than a style's, and not a Component declaration.
        bool isSceneObject(const QObject *object) {
            if (!isNonVisual(object) || object->inherits("QQmlComponent")) return false;
            const QQmlContext *context = qmlContext(object);
            if (!context) return false;
            return !isStyleContext(context) || !object->objectName().isEmpty();
        }

        bool nodeVisible(const QObject *object) {
            if (auto *item = qobject_cast<const QQuickItem *>(object)) return item->isVisible();
            if (isPopup(object)) return object->property("visible").toBool();
            if (auto *window = qobject_cast<const QWindow *>(object)) return window->isVisible();
            // Non-visual objects only count as hidden when they have a visible flag (dialogs).
            return !hasProperty(object, "visible") || object->property("visible").toBool();
        }

        QString slug(const QString &text) {
            static const QRegularExpression separators(QStringLiteral("[^A-Za-z0-9]+"));
            QString result;
            bool upper = false;
            for (const QString &word: stripMnemonics(text).split(separators, Qt::SkipEmptyParts)) {
                result += upper ? word.left(1).toUpper() + word.mid(1) : word.left(1).toLower() + word.mid(1);
                upper = true;
            }
            return result.left(48);
        }

        // ---- value conversion ----------------------------------------------------

        QJsonValue toJson(const QVariant &value, int depth = 0);

        QJsonObject objectSummary(QObject *object) {
            if (!object) return {};
            QJsonObject summary{{"type", cleanTypeName(object->metaObject())}};
            if (!object->objectName().isEmpty()) summary.insert("name", object->objectName());
            if (const QString id = qmlId(object); !id.isEmpty()) summary.insert("id", id);
            return summary;
        }

        QJsonValue toJson(const QVariant &value, const int depth) {
            if (!value.isValid()) return QJsonValue::Null;
            if (depth > 4) return value.toString();
            const int type = value.typeId();
            if (type == qMetaTypeId<QJSValue>()) {
                const auto js = value.value<QJSValue>();
                if (js.isUndefined() || js.isNull()) return QJsonValue::Null;
                return toJson(js.toVariant(), depth + 1);
            }
            switch (type) {
                case QMetaType::Bool: return value.toBool();
                case QMetaType::Int:
                case QMetaType::UInt:
                case QMetaType::LongLong:
                case QMetaType::ULongLong:
                case QMetaType::Short:
                case QMetaType::UShort:
                case QMetaType::Float:
                case QMetaType::Double: {
                    const double number = value.toDouble();
                    return std::isfinite(number) ? QJsonValue(number) : QJsonValue(value.toString());
                }
                case QMetaType::QString: return value.toString();
                case QMetaType::QUrl: return value.toUrl().toString();
                case QMetaType::QDateTime: return value.toDateTime().toString(Qt::ISODateWithMs);
                case QMetaType::QDate: return value.toDate().toString(Qt::ISODate);
                case QMetaType::QColor: {
                    const auto color = value.value<QColor>();
                    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
                }
                case QMetaType::QPoint:
                case QMetaType::QPointF: {
                    const QPointF point = value.toPointF();
                    return QJsonArray{point.x(), point.y()};
                }
                case QMetaType::QSize:
                case QMetaType::QSizeF: {
                    const QSizeF size = value.toSizeF();
                    return QJsonArray{size.width(), size.height()};
                }
                case QMetaType::QRect:
                case QMetaType::QRectF: {
                    const QRectF rect = value.toRectF();
                    return QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()};
                }
                case QMetaType::QStringList: return QJsonArray::fromStringList(value.toStringList());
                case QMetaType::QVariantMap: {
                    QJsonObject object;
                    const auto map = value.toMap();
                    for (auto it = map.cbegin(); it != map.cend(); ++it) object.insert(it.key(), toJson(it.value(), depth + 1));
                    return object;
                }
                default: break;
            }
            if (value.metaType().flags() & QMetaType::PointerToQObject) {
                return objectSummary(value.value<QObject *>());
            }
            if (value.canConvert<QVariantList>() && type != QMetaType::QByteArray) {
                QJsonArray array;
                for (const auto &element: value.value<QVariantList>()) {
                    if (array.size() >= 200) break;
                    array.append(toJson(element, depth + 1));
                }
                return array;
            }
            if (QJsonValue json = QJsonValue::fromVariant(value); !json.isNull()) return json;
            if (value.canConvert<QString>()) return value.toString();
            return QStringLiteral("<%1>").arg(QString::fromLatin1(value.typeName()));
        }

        // ---- argument parsing ----------------------------------------------------

        QString requireString(const QJsonObject &args, const char *key) {
            const QString value = args.value(QLatin1String(key)).toString();
            if (value.isEmpty()) throw DriverError(QStringLiteral("missing required argument '%1'").arg(key));
            return value;
        }

        Qt::KeyboardModifiers parseModifiers(const QString &text) {
            Qt::KeyboardModifiers modifiers;
            for (const auto &part: text.toLower().split(u'+', Qt::SkipEmptyParts)) {
                if (part == u"ctrl" || part == u"control") modifiers |= Qt::ControlModifier;
                else if (part == u"shift") modifiers |= Qt::ShiftModifier;
                else if (part == u"alt") modifiers |= Qt::AltModifier;
                else if (part == u"meta" || part == u"win") modifiers |= Qt::MetaModifier;
                else throw DriverError(QStringLiteral("unknown modifier '%1'").arg(part));
            }
            return modifiers;
        }

        Qt::MouseButton parseButton(const QString &text) {
            if (text.isEmpty() || text == u"left") return Qt::LeftButton;
            if (text == u"right") return Qt::RightButton;
            if (text == u"middle") return Qt::MiddleButton;
            throw DriverError(QStringLiteral("unknown mouse button '%1'").arg(text));
        }

        bool matchesTerm(const QObject *object, const SelectorTerm &term) {
            const auto matches = [&](const QString &value) {
                return !value.isEmpty() && term.pattern.match(value).hasMatch();
            };
            const auto matchesText = [&](const QString &value) { return matches(stripMnemonics(value)); };
            switch (term.kind) {
                case SelectorTerm::Kind::Name: return matches(object->objectName());
                case SelectorTerm::Kind::Id: return matches(qmlId(object));
                case SelectorTerm::Kind::Text:
                    return matchesText(stringProperty(object, "text")) || matchesText(stringProperty(object, "title")) ||
                           matchesText(stringProperty(object, "placeholderText"));
                case SelectorTerm::Kind::RawText:
                    return matches(stringProperty(object, "text")) || matches(stringProperty(object, "title")) ||
                           matches(stringProperty(object, "placeholderText"));
                case SelectorTerm::Kind::Type: {
                    const auto chain = typeChain(object);
                    return std::any_of(chain.cbegin(), chain.cend(), matches);
                }
                case SelectorTerm::Kind::Accessible: return matchesText(accessibleName(const_cast<QObject *>(object)));
                case SelectorTerm::Kind::Object:
                    return isNonVisual(object) && (matches(object->objectName()) || matches(qmlId(object)));
            }
            return false;
        }

        bool matchesSegment(const QObject *object, const SelectorSegment &segment) {
            return std::all_of(segment.terms.cbegin(), segment.terms.cend(),
                               [&](const SelectorTerm &term) { return matchesTerm(object, term); });
        }

        // Nodes worth showing in the default (non-raw) tree: anything an agent could
        // name in a selector or interact with. Uninteresting nodes are elided and
        // their children hoisted.
        bool isInteresting(const QObject *object) {
            static const char *const kInteractiveTypes[] = {
                "QQuickControl", "QQuickPopup", "QQuickTextInput", "QQuickTextEdit", "QQuickMouseArea",
                "QQuickFlickable", "QQuickPaintedItem", "QWindow",
            };
            if (!object->objectName().isEmpty() || !qmlId(object).isEmpty()) return true;
            for (const char *type: kInteractiveTypes) {
                if (object->inherits(type)) return true;
            }
            return !stringProperty(object, "text").isEmpty();
        }
    }

    UiDriver::UiDriver(QQmlEngine *engine, QObject *mainRoot)
        : m_engine(engine), m_mainRoot(mainRoot) {
        m_logCursor = MessageLog::instance().lastSeq();
    }

    QStringList UiDriver::commands() {
        return {"ping", "windows", "tree", "find", "screenshot", "click", "hover", "mouse", "drag", "scroll", "key",
                "type", "get", "set", "invoke", "eval", "wait", "resize", "logs"};
    }

    QJsonObject UiDriver::execute(const QString &command, const QJsonObject &args) {
        const qint64 before = MessageLog::instance().lastSeq();
        m_ownerCacheValid = false;
        QJsonObject result = dispatch(command, args);
        m_ownerCacheValid = false;
        if (command != u"logs") {
            QJsonArray warnings;
            for (const auto &entry: MessageLog::instance().since(before, true, 20)) {
                // Offscreen-QPA chatter (e.g. "This plugin does not support raise()")
                // says nothing about the app; it stays available through `logs`.
                if (entry.toObject().value("message").toString().startsWith(u"This plugin does not support")) continue;
                warnings.append(entry);
            }
            if (!warnings.isEmpty()) result.insert("newWarnings", warnings);
        }
        return result;
    }

    QJsonObject UiDriver::dispatch(const QString &command, const QJsonObject &args) {
        if (!m_mainRoot) throw DriverError(QStringLiteral("the main window no longer exists"));
        if (command == u"ping") {
            // logSeq lets a client see whether the app is still logging without
            // moving the `logs` cursor (the CLI's file commands wait for quiet).
            return {{"pong", true}, {"platform", QGuiApplication::platformName()},
                    {"pid", QCoreApplication::applicationPid()},
                    {"logSeq", static_cast<qint64>(MessageLog::instance().lastSeq())}};
        }
        if (command == u"windows") return cmdWindows();
        if (command == u"tree") return cmdTree(args);
        if (command == u"find") return cmdFind(args);
        if (command == u"screenshot") return cmdScreenshot(args);
        if (command == u"click") return cmdClick(args);
        if (command == u"hover") return cmdHover(args);
        if (command == u"mouse") return cmdMouse(args);
        if (command == u"drag") return cmdDrag(args);
        if (command == u"scroll") return cmdScroll(args);
        if (command == u"key") return cmdKey(args);
        if (command == u"type") return cmdType(args);
        if (command == u"get") return cmdGet(args);
        if (command == u"set") return cmdSet(args);
        if (command == u"invoke") return cmdInvoke(args);
        if (command == u"eval") return cmdEval(args);
        if (command == u"wait") return cmdWait(args);
        if (command == u"resize") return cmdResize(args);
        if (command == u"logs") return cmdLogs(args);
        throw DriverError(QStringLiteral("unknown command '%1'").arg(command),
                          {{"commands", QJsonArray::fromStringList(commands())}});
    }

    // ---- windows -----------------------------------------------------------------

    QQuickWindow *UiDriver::mainWindow() const { return qobject_cast<QQuickWindow *>(m_mainRoot.data()); }

    QList<QQuickWindow *> UiDriver::quickWindows() const {
        QList<QQuickWindow *> windows;
        if (auto *main = mainWindow()) windows.append(main);
        for (QWindow *window: QGuiApplication::topLevelWindows()) {
            auto *quick = qobject_cast<QQuickWindow *>(window);
            if (quick && !windows.contains(quick)) windows.append(quick);
        }
        return windows;
    }

    QList<QObject *> UiDriver::sceneObjects() const {
        QList<QObject *> objects;
        QSet<const QObject *> visited;
        std::function<void(QObject *)> walk = [&](QObject *object) {
            for (QObject *child: object->children()) {
                if (visited.contains(child)) continue;
                visited.insert(child);
                if (isPopup(child) || isSceneObject(child)) objects.append(child);
                if (!qobject_cast<QWindow *>(child)) walk(child);
            }
        };
        for (auto *window: quickWindows()) {
            walk(window);
            if (window->contentItem()) walk(window->contentItem());
        }
        return objects;
    }

    UiDriver::WindowOwner UiDriver::windowOwner(QQuickWindow *window) const {
        if (!m_ownerCacheValid) {
            m_ownerCache.clear();
            m_ownerCacheValid = true;
        }
        if (const auto cached = m_ownerCache.constFind(window); cached != m_ownerCache.cend()) return *cached;

        WindowOwner owner;
        if (window != mainWindow() && window->objectName().isEmpty() && qmlId(window).isEmpty()) {
            for (QObject *parent = window->parent(); parent && !owner.object; parent = parent->parent()) {
                if (isPopup(parent)) owner = {parent, QStringLiteral("popup")};
                else if (isQtDialog(parent)) owner = {parent, QStringLiteral("fallback")};
            }
            const QList<QObject *> objects = owner.object ? QList<QObject *>() : sceneObjects();
            // A Popup the app declared, shown in its own window.
            for (QObject *object: objects) {
                if (!isPopup(object)) continue;
                if (QQuickItem *popupItem = popupItemOf(object); popupItem && popupItem->window() == window) {
                    owner = {object, QStringLiteral("popup")};
                    break;
                }
            }
            // A QtQuick.Dialogs dialog's QML fallback is a Popup (…DialogImpl) its
            // helper creates; report the app's dialog instead. When the impl is not
            // in the app's object tree, its controls belong to its own QML document,
            // whose root is the impl Popup.
            QObject *impl = owner.object && isDialogImpl(owner.object) ? owner.object.data() : nullptr;
            if (!owner.object && window->contentItem()) {
                for (QQuickItem *popupItem: window->contentItem()->childItems()) {
                    for (QQuickItem *child: popupItem->childItems()) {
                        const QQmlContext *context = qmlContext(child);
                        if (QObject *root = context ? context->contextObject() : nullptr; root && isDialogImpl(root)) {
                            impl = root;
                        }
                    }
                }
            }
            if (impl) {
                QObject *dialog = nullptr;
                for (QObject *parent = impl->parent(); parent && !dialog; parent = parent->parent()) {
                    if (isQtDialog(parent)) dialog = parent;
                }
                QList<QObject *> dialogs;
                QList<QObject *> visibleDialogs;
                for (QObject *object: objects.isEmpty() ? sceneObjects() : objects) {
                    if (!isQtDialog(object)) continue;
                    dialogs.append(object);
                    if (object->property("visible").toBool()) visibleDialogs.append(object);
                }
                // Prefer an open dialog with the impl's title, then the only open one,
                // then (for a closed fallback window) any dialog with that title.
                const QString title = impl->property("title").toString();
                const auto titled = [&](const QList<QObject *> &candidates) -> QObject * {
                    for (QObject *candidate: candidates) {
                        if (!title.isEmpty() && candidate->property("title").toString() == title) return candidate;
                    }
                    return nullptr;
                };
                if (!dialog) dialog = titled(visibleDialogs);
                if (!dialog && visibleDialogs.size() == 1) dialog = visibleDialogs.first();
                if (!dialog) dialog = titled(dialogs);
                if (dialog) owner = {dialog, QStringLiteral("fallback")};
            }
        }
        m_ownerCache.insert(window, owner);
        return owner;
    }

    QString UiDriver::dialogKey(QObject *dialog) const {
        if (!dialog->objectName().isEmpty()) return dialog->objectName();
        if (const QString id = qmlId(dialog); !id.isEmpty()) return id;
        if (const QString title = stringProperty(dialog, "title"); !title.isEmpty()) return slug(title);
        return slug(displayText(dialog));
    }

    QList<QObject *> UiDriver::windowlessDialogs() const {
        QSet<QObject *> owned;
        for (auto *window: quickWindows()) {
            if (window->isVisible()) owned.insert(windowOwner(window).object.data());
        }
        QList<QObject *> dialogs;
        for (QObject *object: sceneObjects()) {
            if (isQtDialog(object) && object->property("visible").toBool() && !owned.contains(object)) {
                dialogs.append(object);
            }
        }
        return dialogs;
    }

    QString UiDriver::windowKey(QQuickWindow *window) const {
        if (window == mainWindow()) return QStringLiteral("main");
        if (!window->objectName().isEmpty()) return window->objectName();
        if (const QString id = qmlId(window); !id.isEmpty()) return id;
        if (const WindowOwner owner = windowOwner(window); owner.object) {
            if (const QString key = dialogKey(owner.object); !key.isEmpty()) return key;
        }
        return QStringLiteral("window%1").arg(quickWindows().indexOf(window));
    }

    QQuickWindow *UiDriver::resolveWindow(const QString &key) const {
        const auto windows = quickWindows();
        if (key.isEmpty() || key == u"main") {
            if (auto *main = mainWindow()) return main;
        }
        bool isIndex = false;
        if (const int index = key.toInt(&isIndex); isIndex && index >= 0 && index < windows.size()) {
            return windows[index];
        }
        for (auto *window: windows) {
            if (windowKey(window) == key || window->objectName() == key || qmlId(window) == key ||
                window->title() == key)
                return window;
        }
        for (QObject *dialog: windowlessDialogs()) {
            if (dialogKey(dialog) == key) {
                throw DriverError(QStringLiteral("'%1' is a native dialog: it has no window the driver can capture "
                                                 "or send input to (run headless to get the QML fallback)").arg(key));
            }
        }
        QJsonArray known;
        for (auto *window: windows) known.append(windowRecord(window));
        throw DriverError(QStringLiteral("no window '%1'").arg(key), {{"windows", known}});
    }

    QJsonObject UiDriver::windowRecord(QQuickWindow *window) const {
        QJsonObject record{
            {"key", windowKey(window)},
            {"type", cleanTypeName(window->metaObject())},
            {"title", window->title()},
            {"visible", window->isVisible()},
            {"active", QGuiApplication::focusWindow() == window},
            {"rect", rectJson(QRectF(window->geometry()))},
        };
        if (!window->objectName().isEmpty()) record.insert("name", window->objectName());
        if (const QString id = qmlId(window); !id.isEmpty()) record.insert("id", id);
        if (const WindowOwner owner = windowOwner(window); owner.object) {
            record.insert("kind", owner.kind);
            record.insert("owner", dialogKey(owner.object));
            if (record.value("title").toString().isEmpty()) record.insert("title", displayText(owner.object));
        }
        return record;
    }

    QJsonObject UiDriver::cmdWindows() const {
        QJsonArray windows;
        for (auto *window: quickWindows()) windows.append(windowRecord(window));
        // A visible dialog without a window was shown natively (visible sessions)
        // or inside its parent window; either way there is nothing separate to capture.
        const bool native = QGuiApplication::platformName() == u"windows";
        for (QObject *dialog: windowlessDialogs()) {
            windows.append(QJsonObject{
                {"key", dialogKey(dialog)},
                {"type", cleanTypeName(dialog->metaObject())},
                {"title", displayText(dialog)},
                {"visible", true},
                {"kind", native ? QStringLiteral("native") : QStringLiteral("inline")},
                {"owner", dialogKey(dialog)},
                {"capturable", false},
            });
        }
        return {{"windows", windows}};
    }

    // ---- scene walk and selectors ------------------------------------------------

    UiDriver::Nodes UiDriver::collect(const QString &windowKeyFilter, const bool includeHidden,
                                      const bool withObjects) const {
        Nodes nodes;
        QList<QQuickWindow *> windows;
        if (windowKeyFilter.isEmpty()) windows = quickWindows();
        else windows.append(resolveWindow(windowKeyFilter));

        // Shared across windows: a popup shown in its own window is reached both
        // through its declaring object and as that window's content; keep the first.
        QSet<const QObject *> visited;
        for (auto *window: windows) {
            if (!window->isVisible() && !includeHidden) continue;
            QList<QQuickItem *> skippedPopupItems;

            // Items are walked through their visual children; popups through the
            // QObject tree of whatever declared them (their popup items sit under the
            // window overlay, where they are skipped and picked up as orphans later).
            std::function<void(QObject *, QQuickItem *, int)> visit;
            // Non-visual QML objects (obj: selectors) hang off their declaring object.
            const std::function<void(QObject *, int)> visitObjects = [&](QObject *owner, const int parent) {
                for (QObject *child: owner->children()) {
                    if (isPopup(child)) {
                        visit(child, popupItemOf(child), parent);
                    } else if (isSceneObject(child) && !visited.contains(child)) {
                        visited.insert(child);
                        const int index = static_cast<int>(nodes.size());
                        nodes.push_back({child, nullptr, parent, window});
                        visitObjects(child, index);
                    }
                }
            };
            visit = [&](QObject *object, QQuickItem *item, const int parent) {
                    if (!object || visited.contains(object)) return;
                    visited.insert(object);
                    if (item) visited.insert(item);
                    if (!includeHidden && !nodeVisible(object)) return;
                    if (nodes.size() >= kMaxTreeNodes * 4) return;
                    const int index = static_cast<int>(nodes.size());
                    // A popup may be shown in a window of its own (Popup.Window).
                    auto *itemWindow = item && object != window ? qobject_cast<QQuickWindow *>(item->window()) : nullptr;
                    nodes.push_back({object, item, parent, itemWindow ? itemWindow : window});
                    if (item) {
                        for (QQuickItem *child: item->childItems()) {
                            if (child->inherits("QQuickPopupItem")) {
                                skippedPopupItems.append(child);
                                continue;
                            }
                            visit(child, child, index);
                        }
                    }
                    for (QObject *child: object->children()) {
                        if (isPopup(child)) visit(child, popupItemOf(child), index);
                    }
                    if (withObjects) {
                        visitObjects(object, index);
                        if (item && item != object) visitObjects(item, index);
                    }
                };
            const int rootIndex = static_cast<int>(nodes.size());
            visit(window, window->contentItem(), -1);
            if (static_cast<int>(nodes.size()) == rootIndex) continue;
            for (QQuickItem *orphan: skippedPopupItems) {
                if (!visited.contains(orphan)) visit(orphan, orphan, rootIndex);
            }
        }
        return nodes;
    }

    std::vector<int> UiDriver::resolveAll(const Selector &selector, const Nodes &nodes) const {
        std::vector<int> current;
        if (selector.segments.isEmpty()) {
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                if (nodes[i].parent < 0) current.push_back(i);
            }
            return current;
        }
        for (int segmentIndex = 0; segmentIndex < selector.segments.size(); ++segmentIndex) {
            const auto &segment = selector.segments[segmentIndex];
            std::vector<bool> scope(nodes.size(), false);
            for (const int index: current) scope[index] = true;
            const auto underScope = [&](int index) {
                for (index = nodes[index].parent; index >= 0; index = nodes[index].parent) {
                    if (scope[index]) return true;
                }
                return false;
            };
            std::vector<int> matches;
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                if (segmentIndex > 0 && !underScope(i)) continue;
                if (matchesSegment(nodes[i].object, segment)) matches.push_back(i);
            }
            if (segment.index) {
                int pick = *segment.index;
                if (pick < 0) pick += static_cast<int>(matches.size());
                if (pick < 0 || pick >= static_cast<int>(matches.size())) {
                    throw DriverError(QStringLiteral("selector '%1': index @%2 is out of range (%3 match(es))")
                                          .arg(selector.source).arg(*segment.index).arg(matches.size()));
                }
                matches = {matches[pick]};
            }
            current = std::move(matches);
            if (current.empty()) break;
        }
        return current;
    }

    UiDriver::Node UiDriver::resolveOne(const QString &selectorText, bool includeHidden) const {
        const Selector selector = parseSelector(selectorText);
        // Non-visual objects have no visibility of their own (and often live in
        // closed menus), so obj: selectors search everything.
        const bool objects = selector.targetsObjects();
        includeHidden = includeHidden || objects;
        const Nodes nodes = collect(selector.window, includeHidden, objects);
        std::vector<int> matches = resolveAll(selector, nodes);

        if (matches.empty()) {
            QJsonObject details;
            if (!includeHidden) {
                const Nodes hidden = collect(selector.window, true);
                if (const auto hiddenMatches = resolveAll(selector, hidden); !hiddenMatches.empty()) {
                    details.insert("hiddenMatches", static_cast<int>(hiddenMatches.size()));
                }
            }
            // Suggest names/ids/texts containing the last term's value.
            if (!selector.segments.isEmpty()) {
                QString needle = selector.segments.last().terms.first().value;
                needle = needle.remove(u'*').remove(u'?').toLower();
                QJsonArray suggestions;
                QSet<QString> seen;
                for (const auto &node: nodes) {
                    for (const QString &candidate: {node.object->objectName(), qmlId(node.object),
                                                    displayText(node.object)}) {
                        if (candidate.isEmpty() || seen.contains(candidate) || !candidate.toLower().contains(needle)) continue;
                        seen.insert(candidate);
                        suggestions.append(candidate);
                        if (suggestions.size() >= 12) break;
                    }
                    if (suggestions.size() >= 12) break;
                }
                if (!suggestions.isEmpty()) details.insert("similar", suggestions);
            }
            QString message = QStringLiteral("no visible item matches '%1'").arg(selectorText);
            if (details.contains("hiddenMatches")) {
                message += QStringLiteral(" (%1 hidden item(s) do; pass all=true / --all to include hidden)")
                               .arg(details.value("hiddenMatches").toInt());
            }
            throw DriverError(message, details);
        }

        if (matches.size() > 1) {
            // A control and its own label both match text:...; keep the outermost.
            std::vector<bool> matched(nodes.size(), false);
            for (const int index: matches) matched[index] = true;
            std::erase_if(matches, [&](int index) {
                for (index = nodes[index].parent; index >= 0; index = nodes[index].parent) {
                    if (matched[index]) return true;
                }
                return false;
            });
        }
        if (matches.size() > 1) {
            QJsonArray candidates;
            for (const int index: matches) {
                if (candidates.size() >= kMaxCandidates) break;
                candidates.append(record(nodes[index], true));
            }
            throw DriverError(QStringLiteral("selector '%1' is ambiguous: %2 matches; narrow it or append @N")
                                  .arg(selectorText).arg(matches.size()),
                              {{"candidates", candidates}});
        }
        return nodes[matches.front()];
    }

    QJsonObject UiDriver::record(const Node &node, const bool withWindow) const {
        QObject *object = node.object;
        QJsonObject record{{"type", cleanTypeName(object->metaObject())}};
        if (!object->objectName().isEmpty()) record.insert("name", object->objectName());
        if (const QString id = qmlId(object); !id.isEmpty()) record.insert("id", id);
        const QString text = displayText(object);
        if (!text.isEmpty()) record.insert("text", text.left(200));
        if (const QString placeholder = stringProperty(object, "placeholderText"); !placeholder.isEmpty()) {
            record.insert("placeholder", placeholder);
        }
        if (object != node.window) {
            if (const QString accessible = accessibleName(object); !accessible.isEmpty() && accessible != text) {
                record.insert("acc", accessible.left(200));
            }
        }
        if (!node.item && object != node.window) {
            record.insert("kind", QStringLiteral("object"));  // non-visual: no geometry
        } else {
            const QRectF rect = object == node.window ? QRectF(0, 0, node.window->width(), node.window->height())
                                                      : sceneRect(node.item);
            record.insert("rect", rectJson(rect));
            if (object != node.window && node.item &&
                !rect.intersects(QRectF(0, 0, node.window->width(), node.window->height()))) {
                record.insert("offscreen", true);
            }
        }
        if (!nodeVisible(object)) record.insert("visible", false);
        if (hasProperty(object, "enabled") && !object->property("enabled").toBool()) record.insert("enabled", false);
        if (node.item && node.item->hasActiveFocus() && object != node.window) record.insert("focus", true);
        if (object->property("checkable").toBool()) record.insert("checked", object->property("checked").toBool());
        if (hasProperty(object, "currentIndex")) record.insert("currentIndex", object->property("currentIndex").toInt());
        if (hasProperty(object, "value")) {
            if (const QVariant value = object->property("value"); value.canConvert<double>() &&
                                                                  value.typeId() != QMetaType::QString) {
                record.insert("value", value.toDouble());
            }
        }
        if (withWindow) record.insert("window", windowKey(node.window));
        return record;
    }

    QJsonObject UiDriver::cmdFind(const QJsonObject &args) const {
        const Selector selector = parseSelector(requireString(args, "selector"));
        // objects=true lists only non-visual QML objects (Action, Timer, dialogs, ...).
        const bool onlyObjects = args.value("objects").toBool();
        const bool objects = onlyObjects || selector.targetsObjects();
        const bool includeHidden = args.value("all").toBool() || objects;
        const int limit = args.value("limit").toInt(50);
        const Nodes nodes = collect(selector.window, includeHidden, objects);
        auto matches = resolveAll(selector, nodes);
        if (onlyObjects) std::erase_if(matches, [&](const int index) { return nodes[index].item || !isNonVisual(nodes[index].object); });
        QJsonArray records;
        for (const int index: matches) {
            if (records.size() >= limit) break;
            records.append(record(nodes[index], true));
        }
        return {{"count", static_cast<int>(matches.size())}, {"matches", records}};
    }

    QJsonObject UiDriver::cmdTree(const QJsonObject &args) const {
        const bool includeHidden = args.value("all").toBool();
        const bool raw = args.value("raw").toBool();
        const int maxDepth = args.value("depth").toInt(1000);
        const Selector selector = parseSelector(args.value("selector").toString());
        const Nodes nodes = collect(selector.window, includeHidden);
        std::vector<int> roots = resolveAll(selector, nodes);
        if (!selector.segments.isEmpty() && roots.empty()) {
            throw DriverError(QStringLiteral("no visible item matches '%1'").arg(selector.source));
        }

        std::vector<std::vector<int>> children(nodes.size());
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            if (nodes[i].parent >= 0) children[nodes[i].parent].push_back(i);
        }
        int emitted = 0;
        bool truncated = false;
        std::function<QJsonArray(int, int, const QString &)> build =
            [&](const int index, const int depth, const QString &ancestorText) -> QJsonArray {
                if (emitted >= kMaxTreeNodes) {
                    truncated = true;
                    return {};
                }
                const Node &node = nodes[index];
                const QString text = displayText(node.object);
                const bool unnamed = node.object->objectName().isEmpty() && qmlId(node.object).isEmpty();
                // A control's own label repeats the control's text; drop it.
                const bool duplicateLabel = unnamed && !text.isEmpty() && text == ancestorText &&
                                            !node.object->inherits("QQuickControl");
                const bool show = raw || node.parent < 0 || (isInteresting(node.object) && !duplicateLabel);
                QJsonArray childArray;
                if (depth + (show ? 1 : 0) <= maxDepth) {
                    const QString passText = show && !text.isEmpty() ? text : ancestorText;
                    for (const int child: children[index]) {
                        for (const auto &value: build(child, depth + (show ? 1 : 0), passText)) childArray.append(value);
                    }
                }
                if (!show) return childArray;
                ++emitted;
                QJsonObject entry = record(node, false);
                if (node.parent < 0) entry.insert("window", windowKey(node.window));
                if (!childArray.isEmpty()) entry.insert("children", childArray);
                return {entry};
            };
        QJsonArray tree;
        for (const int root: roots) {
            for (const auto &value: build(root, 0, QString())) tree.append(value);
        }
        QJsonObject result{{"nodes", tree}};
        if (truncated) result.insert("truncated", true);
        return result;
    }

    // ---- screenshots -------------------------------------------------------------

    QJsonObject UiDriver::cmdScreenshot(const QJsonObject &args) const {
        const QString path = requireString(args, "path");
        QQuickWindow *window = nullptr;
        QRectF crop;
        QJsonValue target;
        if (const QString selectorText = args.value("selector").toString(); !selectorText.isEmpty()) {
            const Node node = resolveOne(selectorText, false);
            if (!node.item && node.object != node.window) {
                throw DriverError(QStringLiteral("'%1' is non-visual; there is nothing to crop to").arg(selectorText));
            }
            window = node.window;
            if (node.object != node.window) {
                const int pad = args.value("pad").toInt(8);
                crop = sceneRect(node.item).adjusted(-pad, -pad, pad, pad);
            }
            target = record(node, true);
        } else {
            window = resolveWindow(args.value("window").toString());
        }
        if (!window->isVisible()) {
            throw DriverError(QStringLiteral("window '%1' is not visible").arg(windowKey(window)));
        }
        QImage image = window->grabWindow();
        if (image.isNull()) throw DriverError(QStringLiteral("grabWindow() returned an empty image"));
        QString contextWindow;
        if (args.value("withContext").toBool() && crop.isNull() && window != mainWindow()) {
            // Paint the window over the one it belongs to, where the platform shows it.
            auto *owner = qobject_cast<QQuickWindow *>(window->transientParent());
            if (!owner || !owner->isVisible()) owner = mainWindow();
            if (owner && owner != window && owner->isVisible()) {
                QImage base = owner->grabWindow();
                const qreal ratio = base.devicePixelRatio();
                QPointF offset = QPointF(window->geometry().topLeft() - owner->geometry().topLeft());
                const QRectF ownerRect(QPointF(0, 0), QSizeF(owner->size()));
                // Offscreen windows can sit at arbitrary positions; centre the dialog
                // over its owner when its own position would leave the owner's frame.
                if (!ownerRect.contains(QRectF(offset, QSizeF(window->size())))) {
                    offset = QPointF((owner->width() - window->width()) / 2.0, (owner->height() - window->height()) / 2.0);
                }
                base = base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QPainter painter(&base);
                painter.drawImage(QRectF(offset * ratio, QSizeF(window->size()) * ratio), image);
                painter.end();
                image = base;
                contextWindow = windowKey(owner);
            }
        }
        if (!crop.isNull()) {
            const qreal ratio = image.devicePixelRatio();
            const QRect pixels = QRectF(crop.x() * ratio, crop.y() * ratio, crop.width() * ratio,
                                        crop.height() * ratio).toAlignedRect().intersected(image.rect());
            if (pixels.isEmpty()) throw DriverError(QStringLiteral("the item is outside the window"));
            image = image.copy(pixels);
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!image.save(path)) throw DriverError(QStringLiteral("could not write '%1'").arg(path));

        QJsonArray others;
        for (auto *other: quickWindows()) {
            if (other != window && other->isVisible()) others.append(windowKey(other));
        }
        QJsonObject result{
            {"path", QFileInfo(path).absoluteFilePath()},
            {"window", windowKey(window)},
            {"size", QJsonArray{image.width(), image.height()}},
        };
        if (!target.isNull()) result.insert("target", target);
        if (!others.isEmpty()) result.insert("otherVisibleWindows", others);
        if (!contextWindow.isEmpty()) result.insert("context", contextWindow);
        QJsonArray uncaptured;
        for (QObject *dialog: windowlessDialogs()) uncaptured.append(dialogKey(dialog));
        if (!uncaptured.isEmpty()) result.insert("uncapturedDialogs", uncaptured);
        return result;
    }

    // ---- input -------------------------------------------------------------------

    UiDriver::PointTarget UiDriver::pointTarget(const QJsonValue &spec, const QJsonObject &args) const {
        PointTarget target;
        if (spec.isArray()) {
            const auto point = spec.toArray();
            if (point.size() != 2) throw DriverError(QStringLiteral("a point must be [x, y]"));
            target.window = resolveWindow(args.value("window").toString());
            prepareForInput(target.window);
            target.pos = QPointF(point[0].toDouble(), point[1].toDouble());
            target.target = QJsonArray{point[0], point[1]};
        } else {
            const QString selectorText = spec.toString();
            if (selectorText.isEmpty()) throw DriverError(QStringLiteral("missing a selector or [x, y] target"));
            const Node node = resolveOne(selectorText, false);
            if (!node.item && node.object != node.window) {
                throw DriverError(QStringLiteral("'%1' is a non-visual %2 with no position; use get, set or invoke on it")
                                      .arg(selectorText, cleanTypeName(node.object->metaObject())));
            }
            target.window = node.window;
            // Geometry is only final once the window is exposed at its real size.
            prepareForInput(target.window);
            const QRectF rect = node.object == node.window
                                    ? QRectF(0, 0, node.window->width(), node.window->height())
                                    : sceneRect(node.item);
            target.pos = rect.center();
            if (const auto offset = args.value("offset").toArray(); offset.size() == 2) {
                // An offset is relative to the item's top-left corner.
                target.pos = rect.topLeft() + QPointF(offset[0].toDouble(), offset[1].toDouble());
            }
            target.target = record(node, true);
            target.node = node;
            target.object = node.object;
        }
        const QRectF bounds(0, 0, target.window->width(), target.window->height());
        if (!bounds.contains(target.pos)) {
            throw DriverError(QStringLiteral("target point (%1, %2) is outside window '%3' (%4x%5); scroll it into view first")
                                  .arg(target.pos.x()).arg(target.pos.y()).arg(windowKey(target.window))
                                  .arg(target.window->width()).arg(target.window->height()),
                              {{"target", target.target}});
        }
        return target;
    }

    QJsonObject UiDriver::targetResult(const PointTarget &target) const {
        // The target as resolved: the geometry the input used and its state before
        // the action. A click can hide its own target (a menu item, a dialog
        // button), so the state now is reported separately, only when it changed.
        QJsonObject result{{"target", target.target}};
        if (!target.target.isObject()) return result;
        const QJsonValue after = target.object ? QJsonValue(record(target.node, true))
                                               : QJsonValue(QJsonObject{{"destroyed", true}});
        if (after != target.target) result.insert("after", after);
        return result;
    }

    UiDriver::PointerState &UiDriver::pointer(QQuickWindow *window) {
        // Forget windows that no longer exist (dialog fallbacks come and go).
        const auto windows = quickWindows();
        for (auto it = m_pointers.begin(); it != m_pointers.end();) {
            if (!windows.contains(it.key())) it = m_pointers.erase(it);
            else ++it;
        }
        return m_pointers[window];
    }

    void UiDriver::moveCursor(QQuickWindow *window, const QPointF &to, const int steps) {
        PointerState &state = pointer(window);
        // With nothing known, enter from the window's top-left like a cursor arriving.
        const QPointF from = state.known ? state.pos : QPointF(0, 0);
        const int count = std::max(1, steps);
        for (int step = 1; step <= count; ++step) {
            const QPointF pos = from + (to - from) * (static_cast<qreal>(step) / count);
            QTest::mouseMove(window, pos.toPoint());
            if (step < count) QTest::qWait(16);
        }
        state.pos = to;
        state.known = true;
    }

    void UiDriver::releaseAll() {
        for (auto it = m_pointers.begin(); it != m_pointers.end(); ++it) {
            for (const Qt::MouseButton button: {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
                if (!(it->buttons & button)) continue;
                if (quickWindows().contains(it.key())) {
                    QTest::mouseRelease(it.key(), button, it->modifiers, it->pos.toPoint());
                }
            }
            it->buttons = Qt::NoButton;
        }
    }

    void UiDriver::prepareForInput(QQuickWindow *window) const {
        if (!window->isVisible()) {
            throw DriverError(QStringLiteral("window '%1' is not visible").arg(windowKey(window)));
        }
        // Input sent before the (offscreen) window is exposed is silently dropped.
        if (!window->isExposed() && !QTest::qWaitForWindowExposed(window, 3000)) {
            throw DriverError(QStringLiteral("window '%1' never became exposed").arg(windowKey(window)));
        }
        if (QGuiApplication::focusWindow() != window) {
            window->requestActivate();
            QTest::qWait(20);
        }
    }

    void UiDriver::settle(const QJsonObject &args) {
        QTest::qWait(args.value("settleMs").toInt(kDefaultSettleMs));
    }

    QJsonObject UiDriver::cmdClick(const QJsonObject &args) {
        const QJsonValue spec = args.contains("point") ? args.value("point") : args.value("selector");
        const PointTarget target = pointTarget(spec, args);
        prepareForInput(target.window);
        const Qt::MouseButton button = parseButton(args.value("button").toString());
        const Qt::KeyboardModifiers modifiers = parseModifiers(args.value("modifiers").toString());
        const QPoint pos = target.pos.toPoint();
        moveCursor(target.window, target.pos, 1);
        if (args.value("double").toBool()) QTest::mouseDClick(target.window, button, modifiers, pos);
        else QTest::mouseClick(target.window, button, modifiers, pos);
        settle(args);
        QJsonObject result = targetResult(target);
        result.insert("pos", QJsonArray{pos.x(), pos.y()});
        return result;
    }

    QJsonObject UiDriver::cmdHover(const QJsonObject &args) {
        const QJsonValue spec = args.contains("point") ? args.value("point") : args.value("selector");
        const PointTarget target = pointTarget(spec, args);
        prepareForInput(target.window);
        // A path of moves, not a jump: hover-delayed behaviour (cascading menus,
        // tooltips) tracks the pointer's motion.
        moveCursor(target.window, target.pos, args.value("steps").toInt(8));
        settle(args);
        const QPoint pos = target.pos.toPoint();
        QJsonObject result = targetResult(target);
        result.insert("pos", QJsonArray{pos.x(), pos.y()});
        return result;
    }

    QJsonObject UiDriver::cmdMouse(const QJsonObject &args) {
        const QString action = requireString(args, "action");
        if (action == u"releaseAll") {
            releaseAll();
            settle(args);
            return {{"action", action}};
        }
        if (action != u"press" && action != u"move" && action != u"release") {
            throw DriverError(QStringLiteral("unknown mouse action '%1' (press, move, release)").arg(action));
        }
        // A point or a release with no target stays in the window holding the button.
        QQuickWindow *holding = nullptr;
        for (auto it = m_pointers.cbegin(); it != m_pointers.cend(); ++it) {
            if (it->buttons != Qt::NoButton && quickWindows().contains(it.key())) holding = it.key();
        }
        QJsonObject pointArgs = args;
        if (!args.contains("window") && holding) pointArgs.insert("window", windowKey(holding));

        PointTarget target;
        if (args.contains("point") || !args.value("selector").toString().isEmpty()) {
            target = pointTarget(args.contains("point") ? args.value("point") : args.value("selector"), pointArgs);
        } else if (action == u"release" && holding) {
            target.window = holding;
            target.pos = pointer(holding).pos;
            target.target = QJsonArray{target.pos.x(), target.pos.y()};
        } else {
            throw DriverError(QStringLiteral("mouse %1 needs a selector or a point").arg(action));
        }
        if (holding && target.window != holding) {
            throw DriverError(QStringLiteral("a button is held in window '%1'; the target is in '%2'")
                                  .arg(windowKey(holding), windowKey(target.window)));
        }
        prepareForInput(target.window);
        PointerState &state = pointer(target.window);
        const Qt::MouseButton button = parseButton(args.value("button").toString());
        const Qt::KeyboardModifiers modifiers = args.contains("modifiers")
                                                    ? parseModifiers(args.value("modifiers").toString())
                                                    : state.modifiers;
        const int steps = args.value("steps").toInt(action == u"move" ? 12 : 1);
        moveCursor(target.window, target.pos, steps);
        if (action == u"press") {
            if (state.buttons & button) throw DriverError(QStringLiteral("that button is already held; release it first"));
            QTest::mousePress(target.window, button, modifiers, target.pos.toPoint());
            state.buttons |= button;
            state.modifiers = modifiers;
        } else if (action == u"release") {
            if (!(state.buttons & button)) throw DriverError(QStringLiteral("no held button to release"));
            QTest::mouseRelease(target.window, button, modifiers, target.pos.toPoint());
            state.buttons &= ~Qt::MouseButtons(button);
        }
        settle(args);
        QJsonObject result = targetResult(target);
        result.insert("action", action);
        result.insert("pos", QJsonArray{qRound(target.pos.x()), qRound(target.pos.y())});
        result.insert("window", windowKey(target.window));
        QJsonArray held;
        if (state.buttons & Qt::LeftButton) held.append(QStringLiteral("left"));
        if (state.buttons & Qt::RightButton) held.append(QStringLiteral("right"));
        if (state.buttons & Qt::MiddleButton) held.append(QStringLiteral("middle"));
        result.insert("held", held);
        return result;
    }

    QJsonObject UiDriver::cmdDrag(const QJsonObject &args) {
        const PointTarget from = pointTarget(args.value("from"), args);
        const PointTarget to = pointTarget(args.value("to"), args);
        if (from.window != to.window) throw DriverError(QStringLiteral("drag endpoints are in different windows"));
        prepareForInput(from.window);
        const int steps = std::max(2, args.value("steps").toInt(12));
        const Qt::MouseButton button = parseButton(args.value("button").toString());
        const Qt::KeyboardModifiers modifiers = parseModifiers(args.value("modifiers").toString());
        // press, move and release, as `mouse` does across three commands.
        moveCursor(from.window, from.pos, 1);
        QTest::mousePress(from.window, button, modifiers, from.pos.toPoint());
        moveCursor(from.window, to.pos, steps);
        QTest::qWait(16);
        QTest::mouseRelease(from.window, button, modifiers, to.pos.toPoint());
        settle(args);
        return {{"from", from.target}, {"to", to.target}};
    }

    QJsonObject UiDriver::cmdScroll(const QJsonObject &args) {
        const QJsonValue spec = args.contains("point") ? args.value("point") : args.value("selector");
        const PointTarget target = pointTarget(spec, args);
        prepareForInput(target.window);
        const int dy = args.value("dy").toInt(0);
        const int dx = args.value("dx").toInt(0);
        if (dx == 0 && dy == 0) throw DriverError(QStringLiteral("scroll needs a non-zero dx or dy"));
        const Qt::KeyboardModifiers modifiers = parseModifiers(args.value("modifiers").toString());
        moveCursor(target.window, target.pos, 1);
        // One wheel notch (120) per unit; positive dy scrolls down, positive dx right.
        const int notches = std::max(std::abs(dx), std::abs(dy));
        for (int notch = 0; notch < notches; ++notch) {
            const QPoint angle(notch < std::abs(dx) ? (dx > 0 ? -120 : 120) : 0,
                               notch < std::abs(dy) ? (dy > 0 ? -120 : 120) : 0);
            QWheelEvent event(target.pos, target.window->mapToGlobal(target.pos), QPoint(), angle, Qt::NoButton,
                              modifiers, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(target.window, &event);
            QTest::qWait(16);
        }
        settle(args);
        return targetResult(target);
    }

    QJsonObject UiDriver::cmdKey(const QJsonObject &args) {
        const QString text = requireString(args, "sequence");
        const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (sequence.isEmpty()) throw DriverError(QStringLiteral("could not parse key sequence '%1'").arg(text));
        QQuickWindow *window = nullptr;
        if (args.contains("window")) window = resolveWindow(args.value("window").toString());
        else if (auto *focused = qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow())) window = focused;
        else window = mainWindow();
        prepareForInput(window);
        const int repeat = std::max(1, args.value("repeat").toInt(1));
        for (int i = 0; i < repeat; ++i) QTest::keySequence(window, sequence);
        settle(args);
        QJsonObject result{{"window", windowKey(window)}};
        if (auto *focus = window->activeFocusItem()) result.insert("focus", objectSummary(focus));
        return result;
    }

    QJsonObject UiDriver::cmdType(const QJsonObject &args) {
        const QString text = args.value("text").toString();
        QQuickWindow *window = nullptr;
        if (const QString selectorText = args.value("selector").toString(); !selectorText.isEmpty()) {
            const Node node = resolveOne(selectorText, false);
            if (!node.item) throw DriverError(QStringLiteral("'%1' is not an item that can take focus").arg(selectorText));
            window = node.window;
            prepareForInput(window);
            node.item->forceActiveFocus(Qt::MouseFocusReason);
            // Like clicking at the end of the text: new text is appended unless
            // --clear replaces it or --at-start asks for the old caret-at-0 behaviour.
            QObject *editor = window->activeFocusItem() ? window->activeFocusItem() : node.item;
            if (hasProperty(editor, "cursorPosition")) {
                if (editor->metaObject()->indexOfMethod("deselect()") >= 0) QMetaObject::invokeMethod(editor, "deselect");
                const int length = hasProperty(editor, "length") ? editor->property("length").toInt()
                                                                  : stringProperty(editor, "text").size();
                editor->setProperty("cursorPosition", args.value("atStart").toBool() ? 0 : length);
            }
        } else {
            window = qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow());
            if (!window) window = mainWindow();
            prepareForInput(window);
        }
        if (args.value("clear").toBool()) {
            QTest::keySequence(window, QKeySequence(QKeySequence::SelectAll));
            QTest::keyClick(window, Qt::Key_Backspace);
        }
        // QTest has no QWindow keyClicks(); send each character with its text so
        // non-ASCII input reaches TextInput intact.
        for (const QChar character: text) {
            const auto code = character.unicode() < 0x7f ? static_cast<Qt::Key>(character.toUpper().unicode())
                                                         : Qt::Key_unknown;
            QTest::sendKeyEvent(QTest::Click, window, code, QString(character), Qt::NoModifier);
        }
        if (args.value("enter").toBool()) QTest::keyClick(window, Qt::Key_Return);
        settle(args);
        QJsonObject result{{"window", windowKey(window)}};
        if (auto *focus = window->activeFocusItem()) {
            QJsonObject summary = objectSummary(focus);
            if (const QString value = stringProperty(focus, "text"); !value.isEmpty()) summary.insert("text", value);
            result.insert("focus", summary);
        }
        return result;
    }

    // ---- properties, methods, expressions ------------------------------------------

    QJsonObject UiDriver::cmdGet(const QJsonObject &args) const {
        const Node node = resolveOne(requireString(args, "selector"), args.value("all").toBool());
        const QString name = requireString(args, "property");
        const QQmlProperty property(node.object, name, qmlContext(node.object));
        if (!property.isValid()) {
            QStringList names;
            const QMetaObject *metaObject = node.object->metaObject();
            for (int i = 0; i < metaObject->propertyCount(); ++i) names.append(QString::fromLatin1(metaObject->property(i).name()));
            names.sort();
            throw DriverError(QStringLiteral("%1 has no property '%2'").arg(cleanTypeName(metaObject), name),
                              {{"properties", QJsonArray::fromStringList(names)}});
        }
        return {{"value", toJson(property.read())}, {"target", record(node, true)}};
    }

    QJsonObject UiDriver::cmdSet(const QJsonObject &args) {
        const Node node = resolveOne(requireString(args, "selector"), args.value("all").toBool());
        const QString name = requireString(args, "property");
        if (!args.contains("value")) throw DriverError(QStringLiteral("missing required argument 'value'"));
        QQmlProperty property(node.object, name, qmlContext(node.object));
        if (!property.isValid()) {
            throw DriverError(QStringLiteral("%1 has no property '%2'").arg(cleanTypeName(node.object->metaObject()), name));
        }
        if (!property.write(args.value("value").toVariant())) {
            throw DriverError(QStringLiteral("could not write '%1' (read-only or incompatible value)").arg(name));
        }
        settle(args);
        return {{"value", toJson(property.read())}};
    }

    QJsonObject UiDriver::cmdInvoke(const QJsonObject &args) {
        const Node node = resolveOne(requireString(args, "selector"), args.value("all").toBool());
        const QString method = requireString(args, "method");
        // A parentless object handed to JS without explicit ownership would become
        // garbage-collectable; pin it to C++ ownership first.
        if (!node.object->parent()) QJSEngine::setObjectOwnership(node.object, QJSEngine::CppOwnership);
        QJSValue self = m_engine->newQObject(node.object);
        QJSValue function = self.property(method);
        if (!function.isCallable()) {
            QStringList names;
            const QMetaObject *metaObject = node.object->metaObject();
            for (int i = 0; i < metaObject->methodCount(); ++i) {
                const QMetaMethod meta = metaObject->method(i);
                if (meta.access() == QMetaMethod::Private) continue;
                const QString methodName = QString::fromLatin1(meta.name());
                if (!methodName.startsWith(u'_') && !names.contains(methodName)) names.append(methodName);
            }
            names.sort();
            throw DriverError(QStringLiteral("%1 has no callable '%2'").arg(cleanTypeName(metaObject), method),
                              {{"methods", QJsonArray::fromStringList(names)}});
        }
        QJSValueList jsArgs;
        for (const auto &value: args.value("args").toArray()) jsArgs.append(m_engine->toScriptValue(value.toVariant()));
        const QJSValue returned = function.callWithInstance(self, jsArgs);
        if (returned.isError()) throw DriverError(QStringLiteral("%1() threw: %2").arg(method, returned.toString()));
        settle(args);
        return {{"value", toJson(QVariant::fromValue(returned))}, {"target", record(node, true)}};
    }

    QJsonObject UiDriver::cmdEval(const QJsonObject &args) {
        const QString code = requireString(args, "expression");
        QObject *scope = m_mainRoot;
        if (const QString selectorText = args.value("selector").toString(); !selectorText.isEmpty()) {
            scope = resolveOne(selectorText, args.value("all").toBool()).object;
        }
        QQmlContext *context = qmlContext(scope);
        if (!context) context = m_engine->rootContext();
        QQmlExpression expression(context, scope, code);
        bool undefined = false;
        const QVariant value = expression.evaluate(&undefined);
        if (expression.hasError()) throw DriverError(expression.error().toString());
        settle(args);
        return {{"value", undefined ? QJsonValue(QJsonValue::Null) : toJson(value)}};
    }

    QJsonObject UiDriver::cmdWait(const QJsonObject &args) {
        const int timeout = args.value("timeoutMs").toInt(5000);
        const QString selectorText = args.value("selector").toString();
        const QString propertyName = args.value("property").toString();
        const QString code = args.value("expression").toString();
        const bool gone = args.value("gone").toBool();
        const bool includeHidden = args.value("all").toBool();
        if (selectorText.isEmpty() && code.isEmpty()) {
            throw DriverError(QStringLiteral("wait needs a selector and/or an expression"));
        }
        if (!propertyName.isEmpty() && !args.contains("equals")) {
            throw DriverError(QStringLiteral("wait with a property needs 'equals'"));
        }

        QJsonValue lastValue;
        QString lastError;
        const auto check = [&]() -> bool {
            try {
                QObject *scope = m_mainRoot;
                if (!selectorText.isEmpty()) {
                    const Selector selector = parseSelector(selectorText);
                    const auto nodes = collect(selector.window, includeHidden);
                    const auto matches = resolveAll(selector, nodes);
                    if (gone) return matches.empty();
                    if (matches.empty()) {
                        lastError = QStringLiteral("no match yet");
                        return false;
                    }
                    scope = nodes[matches.front()].object;
                    if (!propertyName.isEmpty()) {
                        lastValue = toJson(QQmlProperty(scope, propertyName, qmlContext(scope)).read());
                        if (lastValue != args.value("equals")) return false;
                    }
                }
                if (!code.isEmpty()) {
                    QQmlContext *context = qmlContext(scope);
                    QQmlExpression expression(context ? context : m_engine->rootContext(), scope, code);
                    const QVariant value = expression.evaluate();
                    if (expression.hasError()) {
                        lastError = expression.error().toString();
                        return false;
                    }
                    lastValue = toJson(value);
                    if (!value.toBool()) return false;
                }
                return true;
            } catch (const DriverError &error) {
                lastError = error.message();
                return false;
            }
        };
        QElapsedTimer timer;
        timer.start();
        if (!QTest::qWaitFor(check, timeout)) {
            QJsonObject details{{"lastValue", lastValue}};
            if (!lastError.isEmpty()) details.insert("lastError", lastError);
            throw DriverError(QStringLiteral("timed out after %1 ms").arg(timeout), details);
        }
        return {{"elapsedMs", static_cast<qint64>(timer.elapsed())}, {"value", lastValue}};
    }

    QJsonObject UiDriver::cmdResize(const QJsonObject &args) {
        QQuickWindow *window = resolveWindow(args.value("window").toString());
        const int width = args.value("width").toInt();
        const int height = args.value("height").toInt();
        if (width <= 0 || height <= 0) throw DriverError(QStringLiteral("resize needs a positive width and height"));
        window->resize(width, height);
        settle(args);
        return {{"window", windowRecord(window)}};
    }

    QJsonObject UiDriver::cmdLogs(const QJsonObject &args) {
        auto &log = MessageLog::instance();
        const bool all = args.value("all").toBool();
        const QJsonArray entries = log.since(all ? 0 : m_logCursor, args.value("warningsOnly").toBool(),
                                             args.value("limit").toInt(200));
        m_logCursor = log.lastSeq();
        if (args.value("clear").toBool()) log.clear();
        return {{"entries", entries}};
    }
}
