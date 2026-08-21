#include "series_expression_editor_model.h"

#include "series_expression_qml.h"

#include <cmath>
#include <type_traits>

namespace ksv::presentation {
    struct SeriesExpressionEditorModel::MutableExprNode {
        QString id;
        QString kind;
        QString primitiveMetric;
        double value = 0.0;
        std::shared_ptr<MutableExprNode> left;
        std::shared_ptr<MutableExprNode> right;
        std::shared_ptr<MutableExprNode> input;
        uint32_t window = 0;
        QString selectionKind;
        uint32_t selectionCount = 0;
        double selectionPercent = 0.0;
    };

    SeriesExpressionEditorModel::SeriesExpressionEditorModel(QObject *parent) : QObject(parent) {}
    SeriesExpressionEditorModel::~SeriesExpressionEditorModel() = default;

    QVariantMap SeriesExpressionEditorModel::root() const { return nodeToEditableMap(m_root); }
    QString SeriesExpressionEditorModel::selectedNodeId() const { return m_selected_node_id; }
    int SeriesExpressionEditorModel::treeRevision() const { return m_tree_revision; }

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

    std::shared_ptr<SeriesExpressionEditorModel::MutableExprNode>
    SeriesExpressionEditorModel::makeNode(const QString &kind) {
        if (!nodeKinds().contains(kind)) return {};
        auto node = std::make_shared<MutableExprNode>();
        node->id = QString("node-%1").arg(m_next_id++);
        node->kind = kind;
        if (kind == "primitive") node->primitiveMetric = primitiveMetrics().front();
        else if (kind == "rollingMean") node->window = 10;
        else if (kind == "averageAcrossRuns") {
            node->selectionKind = "recentRuns";
            node->selectionCount = 5;
        }
        return node;
    }

    bool SeriesExpressionEditorModel::findLocation(const QString &id, const std::shared_ptr<MutableExprNode> &node,
                                                    std::shared_ptr<MutableExprNode> *parent, QString *slot) const {
        if (!node) return false;
        if (node->id == id) return true;
        const auto search = [&](const std::shared_ptr<MutableExprNode> &child, const QString &childSlot) {
            if (!findLocation(id, child, parent, slot)) return false;
            if (parent && !*parent) *parent = node;
            if (slot && slot->isEmpty()) *slot = childSlot;
            return true;
        };
        if (isBinary(node->kind)) return search(node->left, "left") || search(node->right, "right");
        return search(node->input, "input");
    }

    std::shared_ptr<SeriesExpressionEditorModel::MutableExprNode>
    SeriesExpressionEditorModel::findNode(const QString &id) const {
        std::shared_ptr<MutableExprNode> parent;
        QString slot;
        if (!findLocation(id, m_root, &parent, &slot)) return {};
        if (!parent) return m_root;
        return slot == "left" ? parent->left : slot == "right" ? parent->right : parent->input;
    }

    void SeriesExpressionEditorModel::touch() { ++m_tree_revision; emit treeChanged(); }

    void SeriesExpressionEditorModel::select(const QString &id) {
        if (!findNode(id)) return;
        m_selected_node_id = id;
        emit treeChanged();
    }

    void SeriesExpressionEditorModel::replaceChild(const QString &parentId, const QString &slot, const QString &kind) {
        const auto replacement = makeNode(kind);
        if (!replacement) return;
        if (slot == "root") m_root = replacement;
        else {
            const auto parent = findNode(parentId);
            if (!parent || !childSlotsFor(parent->kind).contains(slot)) return;
            if (slot == "left") parent->left = replacement;
            else if (slot == "right") parent->right = replacement;
            else parent->input = replacement;
        }
        m_selected_node_id = replacement->id;
        touch();
    }

    void SeriesExpressionEditorModel::deleteNode(const QString &id) {
        std::shared_ptr<MutableExprNode> parent;
        QString slot;
        if (!findLocation(id, m_root, &parent, &slot)) return;
        if (!parent) {
            m_root.reset();
            m_selected_node_id.clear();
        } else {
            if (slot == "left") parent->left.reset();
            else if (slot == "right") parent->right.reset();
            else parent->input.reset();
            m_selected_node_id = parent->id;
        }
        touch();
    }

    void SeriesExpressionEditorModel::wrapSelected(const QString &kind) {
        const auto selected = findNode(m_selected_node_id);
        const auto wrapper = makeNode(kind);
        if (!selected || !wrapper) return;
        std::shared_ptr<MutableExprNode> parent;
        QString slot;
        findLocation(m_selected_node_id, m_root, &parent, &slot);
        if (isBinary(kind)) wrapper->left = selected;
        else if (childSlotsFor(kind).contains("input")) wrapper->input = selected;
        else return;
        if (!parent) m_root = wrapper;
        else if (slot == "left") parent->left = wrapper;
        else if (slot == "right") parent->right = wrapper;
        else parent->input = wrapper;
        m_selected_node_id = wrapper->id;
        touch();
    }

    void SeriesExpressionEditorModel::changeBinaryOperator(const QString &id, const QString &kind) {
        const auto node = findNode(id);
        if (!node || !isBinary(node->kind) || !isBinary(kind)) return;
        node->kind = kind;
        touch();
    }

    void SeriesExpressionEditorModel::updateField(const QString &id, const QString &field, const QVariant &value) {
        const auto node = findNode(id);
        if (!node) return;
        if (node->kind == "primitive" && field == "metric" && primitiveMetrics().contains(value.toString())) node->primitiveMetric = value.toString();
        else if (node->kind == "constant" && field == "value") node->value = value.toDouble();
        else if (node->kind == "rollingMean" && field == "window") node->window = value.toUInt();
        else if (node->kind == "averageAcrossRuns" && field == "count" && node->selectionKind == "recentRuns") node->selectionCount = value.toUInt();
        else if (node->kind == "averageAcrossRuns" && field == "percent" && node->selectionKind == "topPercentile") node->selectionPercent = value.toDouble();
        else return;
        touch();
    }

    void SeriesExpressionEditorModel::changeSelectionKind(const QString &id, const QString &kind) {
        const auto node = findNode(id);
        if (!node || node->kind != "averageAcrossRuns" || (kind != "recentRuns" && kind != "topPercentile")) return;
        node->selectionKind = kind;
        node->selectionCount = kind == "recentRuns" ? 5 : 0;
        node->selectionPercent = kind == "topPercentile" ? 10.0 : 0.0;
        touch();
    }

    QVariantMap SeriesExpressionEditorModel::nodeToEditableMap(const std::shared_ptr<MutableExprNode> &node) const {
        if (!node) return {};
        QVariantMap map{{"id", node->id}, {"kind", node->kind}};
        if (node->kind == "primitive") map["metric"] = node->primitiveMetric;
        else if (node->kind == "constant") map["value"] = node->value;
        else if (node->kind == "rollingMean") { map["window"] = node->window; map["input"] = nodeToEditableMap(node->input); }
        else if (node->kind == "averageAcrossRuns") {
            map["input"] = nodeToEditableMap(node->input);
            map["selection"] = node->selectionKind == "recentRuns"
                ? QVariantMap{{"kind", "recentRuns"}, {"count", node->selectionCount}}
                : QVariantMap{{"kind", "topPercentile"}, {"percent", node->selectionPercent}};
        } else if (isBinary(node->kind)) { map["left"] = nodeToEditableMap(node->left); map["right"] = nodeToEditableMap(node->right); }
        else if (childSlotsFor(node->kind).contains("input")) map["input"] = nodeToEditableMap(node->input);
        return map;
    }

    QVariantMap SeriesExpressionEditorModel::nodeToPersistenceMap(const std::shared_ptr<MutableExprNode> &node) const {
        if (!node) return {};
        QVariantMap map{{"kind", node->kind}};
        if (node->kind == "primitive") map["primitiveMetric"] = node->primitiveMetric;
        else if (node->kind == "constant") map["value"] = node->value;
        else if (node->kind == "rollingMean") { map["window"] = node->window; map["input"] = nodeToPersistenceMap(node->input); }
        else if (node->kind == "averageAcrossRuns") {
            map["input"] = nodeToPersistenceMap(node->input);
            map["selection"] = node->selectionKind == "recentRuns"
                ? QVariantMap{{"kind", "recentRuns"}, {"count", node->selectionCount}}
                : QVariantMap{{"kind", "topPercentile"}, {"percent", node->selectionPercent}};
        } else if (isBinary(node->kind)) { map["left"] = nodeToPersistenceMap(node->left); map["right"] = nodeToPersistenceMap(node->right); }
        else if (childSlotsFor(node->kind).contains("input")) map["input"] = nodeToPersistenceMap(node->input);
        return map;
    }

    QVariantMap SeriesExpressionEditorModel::toExpressionMap() const { return nodeToPersistenceMap(m_root); }
    std::optional<application::Expression> SeriesExpressionEditorModel::toExpression() const { return parseExpression(toExpressionMap()); }

    bool SeriesExpressionEditorModel::pathTo(const QString &id, const std::shared_ptr<MutableExprNode> &node,
                                              QList<std::shared_ptr<MutableExprNode>> &path) const {
        if (!node) return false;
        path.append(node);
        if (node->id == id) return true;
        const bool found = isBinary(node->kind)
            ? pathTo(id, node->left, path) || pathTo(id, node->right, path)
            : pathTo(id, node->input, path);
        if (!found) path.removeLast();
        return found;
    }

    QVariantList SeriesExpressionEditorModel::ancestorChain(const QString &id) const {
        QList<std::shared_ptr<MutableExprNode>> path;
        if (!pathTo(id, m_root, path)) return {};
        QVariantList result;
        for (const auto &node: path) result.append(nodeToEditableMap(node));
        return result;
    }

    QString SeriesExpressionEditorModel::describe(const QVariantMap &node) const {
        if (node.isEmpty()) return QString::fromUtf8("…");
        const auto kind = node.value("kind").toString();
        if (kind == "primitive") return node.value("metric").toString();
        if (kind == "constant") return QString::number(node.value("value").toDouble());
        if (isBinary(kind)) {
            const auto symbols = QMap<QString, QString>{{"add", "+"}, {"subtract", QString::fromUtf8("−")}, {"multiply", QString::fromUtf8("×")}, {"divide", QString::fromUtf8("÷")}};
            return describe(node.value("left").toMap()) + " " + symbols.value(kind) + " " + describe(node.value("right").toMap());
        }
        const auto label = QMap<QString, QString>{{"runningSum", "RunningSum"}, {"rollingMean", "RollingMean"}, {"projectedFinalValue", "ProjectedFinalValue"}, {"projectRateToFinal", "ProjectRateToFinal"}, {"averageAcrossRuns", "AverageAcrossRuns"}}.value(kind);
        if (kind == "rollingMean") return QString("%1(%2, window: %3)").arg(label, describe(node.value("input").toMap())).arg(node.value("window").toUInt());
        if (kind == "averageAcrossRuns") {
            const auto selection = node.value("selection").toMap();
            const auto text = selection.value("kind").toString() == "recentRuns"
                ? QString("recent %1").arg(selection.value("count").toUInt())
                : QString("top %1%").arg(selection.value("percent").toDouble());
            return QString("%1(%2, over: %3)").arg(label, describe(node.value("input").toMap()), text);
        }
        return QString("%1(%2)").arg(label, describe(node.value("input").toMap()));
    }

    std::shared_ptr<SeriesExpressionEditorModel::MutableExprNode>
    SeriesExpressionEditorModel::nodeFromExpression(const application::Expression &expression) {
        if (!expression) return {};
        return std::visit([this](const auto &value) -> std::shared_ptr<MutableExprNode> {
            using Node = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Node, application::PrimitiveReference>) {
                auto node = makeNode("primitive"); node->primitiveMetric = primitiveMetrics().at(static_cast<int>(value.metric)); return node;
            } else if constexpr (std::is_same_v<Node, application::NumericConstant>) {
                auto node = makeNode("constant"); node->value = value.value; return node;
            } else if constexpr (std::is_same_v<Node, application::Add> || std::is_same_v<Node, application::Subtract> || std::is_same_v<Node, application::Multiply> || std::is_same_v<Node, application::Divide>) {
                const auto kind = std::is_same_v<Node, application::Add> ? "add" : std::is_same_v<Node, application::Subtract> ? "subtract" : std::is_same_v<Node, application::Multiply> ? "multiply" : "divide";
                auto node = makeNode(kind); node->left = nodeFromExpression(value.left); node->right = nodeFromExpression(value.right); return node;
            } else {
                const auto kind = std::is_same_v<Node, application::RunningSum> ? "runningSum" : std::is_same_v<Node, application::RollingMean> ? "rollingMean" : std::is_same_v<Node, application::ProjectedFinalValue> ? "projectedFinalValue" : std::is_same_v<Node, application::ProjectRateToFinal> ? "projectRateToFinal" : "averageAcrossRuns";
                auto node = makeNode(kind); node->input = nodeFromExpression(value.input);
                if constexpr (std::is_same_v<Node, application::RollingMean>) node->window = value.window;
                if constexpr (std::is_same_v<Node, application::AverageAcrossRuns>) std::visit([&](const auto &selection) {
                    using Selection = std::decay_t<decltype(selection)>;
                    if constexpr (std::is_same_v<Selection, application::RecentRuns>) { node->selectionKind = "recentRuns"; node->selectionCount = selection.count; }
                    else { node->selectionKind = "topPercentile"; node->selectionPercent = selection.percent; }
                }, value.selection);
                return node;
            }
        }, expression->value());
    }

    void SeriesExpressionEditorModel::loadFrom(const application::Expression &expression) {
        m_next_id = 1;
        m_root = nodeFromExpression(expression);
        m_selected_node_id.clear();
        touch();
    }
}
