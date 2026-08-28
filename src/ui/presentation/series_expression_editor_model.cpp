#include "series_expression_editor_model.h"

#include "series_expression_qml.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMetaMethod>
#include <QMetaProperty>
#include <type_traits>

#include "app/contracts/expression_dsl.h"

namespace ksv::presentation {
    SeriesExpressionEditorModel::SeriesExpressionEditorModel(QObject *parent) : QObject(parent) {}
    SeriesExpressionEditorModel::~SeriesExpressionEditorModel() = default;

    QStringList SeriesExpressionEditorModel::nodeKinds() const {
        return {"primitive", "constant", "add", "subtract", "multiply", "divide", "runningSum", "rollingMean",
                "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns"};
    }

    QStringList SeriesExpressionEditorModel::primitiveMetrics() const {
        return {"score", "shots", "hits", "kills", "dmg"};
    }

    bool SeriesExpressionEditorModel::isBinary(const QString &kind) const {
        return kind == "add" || kind == "subtract" || kind == "multiply" || kind == "divide";
    }

    QStringList SeriesExpressionEditorModel::childSlotsFor(const QString &kind) const {
        if (isBinary(kind)) return {"left", "right"};
        if (kind == "runningSum" || kind == "rollingMean" || kind == "projectedFinalValue" ||
            kind == "projectRateToFinal" || kind == "averageAcrossRuns") return {"input"};
        return {};
    }

    EditableExpressionNode *SeriesExpressionEditorModel::makeNode(const QString &kind) {
        if (kind == "primitive") return new EditablePrimitiveNode();
        if (kind == "constant") return new EditableConstantNode();
        if (kind == "add" || kind == "subtract" || kind == "multiply" || kind == "divide") {
            auto *node = new EditableBinaryOpNode();
            node->setKind(kind);
            return node;
        }
        if (kind == "rollingMean") return new EditableRollingMeanNode();
        if (kind == "averageAcrossRuns") return new EditableAverageAcrossRunsNode();
        if (kind == "runningSum" || kind == "projectedFinalValue" || kind == "projectRateToFinal") {
            auto *node = new EditableUnaryOpNode();
            node->setKind(kind);
            return node;
        }
        return nullptr;
    }

    QString SeriesExpressionEditorModel::slotOf(EditableExpressionNode *parent, EditableExpressionNode *child) {
        if (auto *binary = qobject_cast<EditableBinaryOpNode *>(parent)) {
            if (binary->left() == child) return "left";
            if (binary->right() == child) return "right";
            return {};
        }
        if (qobject_cast<EditableUnaryOpNode *>(parent) || qobject_cast<EditableRollingMeanNode *>(parent) ||
            qobject_cast<EditableAverageAcrossRunsNode *>(parent)) return "input";
        return {};
    }

    bool SeriesExpressionEditorModel::setChildInParent(EditableExpressionNode *parent, const QString &slot,
                                                        EditableExpressionNode *node) {
        if (auto *binary = qobject_cast<EditableBinaryOpNode *>(parent); binary && slot == "left") {
            binary->setLeft(node);
            return true;
        }
        if (auto *binary = qobject_cast<EditableBinaryOpNode *>(parent); binary && slot == "right") {
            binary->setRight(node);
            return true;
        }
        if (auto *unary = qobject_cast<EditableUnaryOpNode *>(parent); unary && slot == "input") {
            unary->setInput(node);
            return true;
        }
        if (auto *rollingMean = qobject_cast<EditableRollingMeanNode *>(parent); rollingMean && slot == "input") {
            rollingMean->setInput(node);
            return true;
        }
        if (auto *average = qobject_cast<EditableAverageAcrossRunsNode *>(parent); average && slot == "input") {
            average->setInput(node);
            return true;
        }
        return false;
    }

    void SeriesExpressionEditorModel::setRoot(EditableExpressionNode *node) {
        if (m_root.get() == node) return;
        m_root.reset(node);
        if (m_root) m_root->setParentNode(nullptr);
        emit rootChanged();
    }

    void SeriesExpressionEditorModel::setSelected(EditableExpressionNode *node) {
        if (m_selected == node) return;
        m_selected = node;
        emit selectedChanged();
    }

    void SeriesExpressionEditorModel::select(EditableExpressionNode *node) { setSelected(node); }

    void SeriesExpressionEditorModel::replaceChild(EditableExpressionNode *parent, const QString &slot,
                                                    const QString &kind) {
        auto *replacement = makeNode(kind);
        if (!replacement) return;
        if (!parent) {
            if (slot != "root") {
                delete replacement;
                return;
            }
        } else if (!childSlotsFor(parent->kind()).contains(slot)) {
            delete replacement;
            return;
        }
        setSelected(nullptr);
        if (!parent) setRoot(replacement);
        else setChildInParent(parent, slot, replacement);
        setSelected(replacement);
        refreshDslObservers();
    }

    std::unique_ptr<EditableExpressionNode> SeriesExpressionEditorModel::detachFromParent(EditableExpressionNode *node) {
        if (!node) return nullptr;
        auto *parent = node->parentNode();
        if (!parent) {
            if (m_root.get() != node) return nullptr;
            auto detached = std::move(m_root);
            emit rootChanged();
            return detached;
        }
        const auto slot = slotOf(parent, node);
        if (auto *binary = qobject_cast<EditableBinaryOpNode *>(parent)) {
            if (slot == "left") return binary->takeLeft();
            if (slot == "right") return binary->takeRight();
        } else if (auto *unary = qobject_cast<EditableUnaryOpNode *>(parent); unary && slot == "input") {
            return unary->takeInput();
        } else if (auto *rollingMean = qobject_cast<EditableRollingMeanNode *>(parent); rollingMean && slot == "input") {
            return rollingMean->takeInput();
        } else if (auto *average = qobject_cast<EditableAverageAcrossRunsNode *>(parent); average && slot == "input") {
            return average->takeInput();
        }
        return nullptr;
    }

    void SeriesExpressionEditorModel::deleteNode(EditableExpressionNode *node) {
        if (!node) return;
        auto *parent = node->parentNode();
        auto detached = detachFromParent(node);
        if (!detached) return;
        setSelected(parent);
        refreshDslObservers();
    }

    void SeriesExpressionEditorModel::wrapSelected(const QString &kind) {
        if (!m_selected) return;
        auto *wrapper = makeNode(kind);
        if (!wrapper) return;
        const bool wrapperHasChildSlot = isBinary(kind) || childSlotsFor(kind).contains("input");
        if (!wrapperHasChildSlot) {
            delete wrapper;
            return;
        }
        auto *parent = m_selected->parentNode();
        const auto slot = parent ? slotOf(parent, m_selected) : QString();
        std::unique_ptr<EditableExpressionNode> detachedSelected;
        if (parent) {
            detachedSelected = detachFromParent(m_selected);
        } else if (m_root.get() == m_selected) {
            detachedSelected = std::move(m_root);
        }
        if (!detachedSelected) {
            delete wrapper;
            return;
        }
        if (auto *binary = qobject_cast<EditableBinaryOpNode *>(wrapper)) {
            binary->setLeft(detachedSelected.release());
        } else if (auto *unary = qobject_cast<EditableUnaryOpNode *>(wrapper)) {
            unary->setInput(detachedSelected.release());
        } else if (auto *rollingMean = qobject_cast<EditableRollingMeanNode *>(wrapper)) {
            rollingMean->setInput(detachedSelected.release());
        } else if (auto *average = qobject_cast<EditableAverageAcrossRunsNode *>(wrapper)) {
            average->setInput(detachedSelected.release());
        }
        if (!parent) setRoot(wrapper);
        else setChildInParent(parent, slot, wrapper);
        setSelected(wrapper);
        refreshDslObservers();
    }

    QString SeriesExpressionEditorModel::describe(EditableExpressionNode *node) const {
        return node ? node->describe() : QString::fromUtf8("…");
    }

    QVariantList SeriesExpressionEditorModel::ancestorChain(EditableExpressionNode *node) const {
        if (!node) return {};
        QVariantList path;
        for (auto *current = node; current; current = current->parentNode())
            path.prepend(QVariant::fromValue(current));
        if (path.isEmpty() || path.first().value<EditableExpressionNode *>() != m_root.get()) return {};
        return path;
    }

    QVariantMap SeriesExpressionEditorModel::nodeToPersistenceMap(EditableExpressionNode *node) const {
        if (!node) return {};
        QVariantMap map{{"kind", node->kind()}};
        if (auto *primitive = qobject_cast<EditablePrimitiveNode *>(node)) {
            map["primitiveMetric"] = primitive->metric();
        } else if (auto *constant = qobject_cast<EditableConstantNode *>(node)) {
            map["value"] = constant->value();
        } else if (auto *binary = qobject_cast<EditableBinaryOpNode *>(node)) {
            map["left"] = nodeToPersistenceMap(binary->left());
            map["right"] = nodeToPersistenceMap(binary->right());
        } else if (auto *unary = qobject_cast<EditableUnaryOpNode *>(node)) {
            map["input"] = nodeToPersistenceMap(unary->input());
        } else if (auto *rollingMean = qobject_cast<EditableRollingMeanNode *>(node)) {
            map["window"] = rollingMean->window();
            map["input"] = nodeToPersistenceMap(rollingMean->input());
        } else if (auto *average = qobject_cast<EditableAverageAcrossRunsNode *>(node)) {
            map["input"] = nodeToPersistenceMap(average->input());
            map["selection"] = average->selectionKind() == "recentRuns"
                ? QVariantMap{{"kind", "recentRuns"}, {"count", average->count()}}
                : QVariantMap{{"kind", "topPercentile"}, {"percent", average->percent()}};
        }
        return map;
    }

    QVariantMap SeriesExpressionEditorModel::toExpressionMap() const { return nodeToPersistenceMap(m_root.get()); }

    std::optional<application::Expression> SeriesExpressionEditorModel::toExpression() const {
        return parseExpression(toExpressionMap());
    }

    EditableExpressionNode *SeriesExpressionEditorModel::nodeFromExpression(const application::Expression &expression) {
        if (!expression) return nullptr;
        return std::visit([this](const auto &value) -> EditableExpressionNode * {
            using Node = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Node, application::PrimitiveReference>) {
                auto *node = new EditablePrimitiveNode();
                node->setMetric(primitiveMetrics().at(static_cast<int>(value.metric)));
                return node;
            } else if constexpr (std::is_same_v<Node, application::NumericConstant>) {
                auto *node = new EditableConstantNode();
                node->setValue(value.value);
                return node;
            } else if constexpr (std::is_same_v<Node, application::Add> || std::is_same_v<Node, application::Subtract> ||
                                 std::is_same_v<Node, application::Multiply> || std::is_same_v<Node, application::Divide>) {
                auto *node = new EditableBinaryOpNode();
                node->setKind(std::is_same_v<Node, application::Add> ? "add"
                    : std::is_same_v<Node, application::Subtract> ? "subtract"
                    : std::is_same_v<Node, application::Multiply> ? "multiply" : "divide");
                node->setLeft(nodeFromExpression(value.left));
                node->setRight(nodeFromExpression(value.right));
                return node;
            } else if constexpr (std::is_same_v<Node, application::RollingMean>) {
                auto *node = new EditableRollingMeanNode();
                node->setWindow(value.window);
                node->setInput(nodeFromExpression(value.input));
                return node;
            } else if constexpr (std::is_same_v<Node, application::AverageAcrossRuns>) {
                auto *node = new EditableAverageAcrossRunsNode();
                node->setInput(nodeFromExpression(value.input));
                std::visit([&](const auto &selection) {
                    using Selection = std::decay_t<decltype(selection)>;
                    if constexpr (std::is_same_v<Selection, application::RecentRuns>) {
                        node->setSelectionKind("recentRuns");
                        node->setCount(selection.count);
                    } else {
                        node->setSelectionKind("topPercentile");
                        node->setPercent(selection.percent);
                    }
                }, value.selection);
                return node;
            } else {
                auto *node = new EditableUnaryOpNode();
                node->setKind(std::is_same_v<Node, application::RunningSum> ? "runningSum"
                    : std::is_same_v<Node, application::ProjectedFinalValue> ? "projectedFinalValue"
                    : "projectRateToFinal");
                node->setInput(nodeFromExpression(value.input));
                return node;
            }
        }, expression->value());
    }

    void SeriesExpressionEditorModel::observeSubtree(EditableExpressionNode *node) {
        if (!node) return;
        static const QMetaMethod dslChanged = QMetaMethod::fromSignal(&SeriesExpressionEditorModel::dslTextChanged);
        const QMetaObject *meta = node->metaObject();
        for (int index = 0; index < meta->propertyCount(); ++index) {
            if (const QMetaProperty property = meta->property(index); property.hasNotifySignal())
                connect(node, property.notifySignal(), this, dslChanged, Qt::UniqueConnection);
        }
        for (const QString &slot: childSlotsFor(node->kind()))
            observeSubtree(node->property(slot.toUtf8().constData()).value<EditableExpressionNode *>());
    }

    void SeriesExpressionEditorModel::refreshDslObservers() {
        observeSubtree(m_root.get());
        emit dslTextChanged();
    }

    void SeriesExpressionEditorModel::loadFrom(const application::Expression &expression) {
        setSelected(nullptr);
        setRoot(nodeFromExpression(expression));
        refreshDslObservers();
    }

    QString SeriesExpressionEditorModel::toDslText() const {
        const auto expression = toExpression();
        if (!expression) return {};
        return QString::fromStdString(application::encodeExpressionDsl(*expression));
    }

    bool SeriesExpressionEditorModel::applyDslText(const QString &text) {
        const auto expression = application::decodeExpressionDsl(text.toStdString());
        if (!expression) return false;
        loadFrom(*expression);
        return true;
    }

    void SeriesExpressionEditorModel::copyToClipboard() const {
        if (auto *clipboard = QGuiApplication::clipboard()) clipboard->setText(toDslText());
    }

    bool SeriesExpressionEditorModel::pasteFromClipboard() {
        auto *clipboard = QGuiApplication::clipboard();
        return clipboard && applyDslText(clipboard->text());
    }
}
