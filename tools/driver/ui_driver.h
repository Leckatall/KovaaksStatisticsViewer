#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <vector>

#include "selector.h"

class QObject;
class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace ksv::driver {
    // In-process UI automation over a loaded QML scene: window and item-tree
    // inspection, selector resolution, synthesized mouse/keyboard input,
    // property access, JS evaluation and window screenshots. Every command
    // takes and returns JSON so the TCP server (driver_server.h) is a thin
    // transport and tests can call execute() directly. See tools/driver/README.md.
    class UiDriver {
    public:
        UiDriver(QQmlEngine *engine, QObject *mainRoot);

        // Runs one protocol command; throws DriverError on failure. Any warnings
        // the app logged while it ran are attached as "newWarnings".
        QJsonObject execute(const QString &command, const QJsonObject &args);

        [[nodiscard]] static QStringList commands();

    private:
        // One entry of a flattened scene walk: a QQuickItem, a Popup (whose visual
        // is its popup item), a window's root (object = the window), or, when a
        // selector asks for obj:, a non-visual QML object (item = nullptr).
        struct Node {
            QObject *object = nullptr;
            QQuickItem *item = nullptr;
            int parent = -1;
            QQuickWindow *window = nullptr;
        };
        using Nodes = std::vector<Node>;

        struct PointTarget {
            QQuickWindow *window = nullptr;
            QPointF pos;
            QJsonValue target;
            Node node;                 // set when the target is an item
            QPointer<QObject> object;  // guards node after the action ran
        };

        // Where the synthesized cursor is and which buttons it holds, per window,
        // so gestures can span commands (mouse press ... shot ... mouse release).
        struct PointerState {
            QPointF pos;
            bool known = false;
            Qt::MouseButtons buttons;
            Qt::KeyboardModifiers modifiers;
        };

        // What owns a top-level window that the app did not declare itself: the
        // Popup shown in it (kind "popup"), or the QtQuick.Dialogs dialog whose
        // QML fallback it is (kind "fallback").
        struct WindowOwner {
            QPointer<QObject> object;
            QString kind;
        };

        QJsonObject dispatch(const QString &command, const QJsonObject &args);

        QJsonObject cmdWindows() const;
        QJsonObject cmdTree(const QJsonObject &args) const;
        QJsonObject cmdFind(const QJsonObject &args) const;
        QJsonObject cmdScreenshot(const QJsonObject &args) const;
        QJsonObject cmdClick(const QJsonObject &args);
        QJsonObject cmdHover(const QJsonObject &args);
        QJsonObject cmdMouse(const QJsonObject &args);
        QJsonObject cmdDrag(const QJsonObject &args);
        QJsonObject cmdScroll(const QJsonObject &args);
        QJsonObject cmdKey(const QJsonObject &args);
        QJsonObject cmdType(const QJsonObject &args);
        QJsonObject cmdGet(const QJsonObject &args) const;
        QJsonObject cmdSet(const QJsonObject &args);
        QJsonObject cmdInvoke(const QJsonObject &args);
        QJsonObject cmdEval(const QJsonObject &args);
        QJsonObject cmdWait(const QJsonObject &args);
        QJsonObject cmdResize(const QJsonObject &args);
        QJsonObject cmdLogs(const QJsonObject &args);

        [[nodiscard]] QList<QQuickWindow *> quickWindows() const;
        [[nodiscard]] QQuickWindow *mainWindow() const;
        [[nodiscard]] QString windowKey(QQuickWindow *window) const;
        [[nodiscard]] QQuickWindow *resolveWindow(const QString &key) const;
        [[nodiscard]] QJsonObject windowRecord(QQuickWindow *window) const;
        [[nodiscard]] WindowOwner windowOwner(QQuickWindow *window) const;
        // Every QML-created QObject reachable from the windows' object trees.
        [[nodiscard]] QList<QObject *> sceneObjects() const;
        // Visible QtQuick.Dialogs dialogs that have no window of their own (native).
        [[nodiscard]] QList<QObject *> windowlessDialogs() const;
        [[nodiscard]] QString dialogKey(QObject *dialog) const;

        [[nodiscard]] Nodes collect(const QString &windowKey, bool includeHidden, bool withObjects = false) const;
        [[nodiscard]] std::vector<int> resolveAll(const Selector &selector, const Nodes &nodes) const;
        [[nodiscard]] Node resolveOne(const QString &selectorText, bool includeHidden) const;
        [[nodiscard]] QJsonObject record(const Node &node, bool withWindow) const;

        [[nodiscard]] PointTarget pointTarget(const QJsonValue &spec, const QJsonObject &args) const;
        // The target as resolved (before the action) plus, when it changed, an
        // "after" record of its state now.
        [[nodiscard]] QJsonObject targetResult(const PointTarget &target) const;
        void prepareForInput(QQuickWindow *window) const;
        static void settle(const QJsonObject &args);

        // Moves the synthesized cursor to `to` through `steps` intermediate moves
        // from its last position in `window`, as a real pointer would.
        void moveCursor(QQuickWindow *window, const QPointF &to, int steps);
        PointerState &pointer(QQuickWindow *window);
        void releaseAll();

        QQmlEngine *m_engine;
        QPointer<QObject> m_mainRoot;
        qint64 m_logCursor = 0;
        QHash<QQuickWindow *, PointerState> m_pointers;
        // Window ownership is recomputed once per command (see execute()).
        mutable QHash<QQuickWindow *, WindowOwner> m_ownerCache;
        mutable bool m_ownerCacheValid = false;
    };
}
