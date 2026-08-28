#include "editable_expression_node.h"

#include <QDebug>
#include <QMap>
#include <QQmlEngine>

namespace ksv::presentation {
    void EditablePrimitiveNode::setMetric(const QString &metric) {
        if (m_metric == metric) return;
        m_metric = metric;
        emit metricChanged();
    }

    void EditableConstantNode::setValue(double value) {
        if (m_value == value) return;
        m_value = value;
        emit valueChanged();
    }

    void EditableBinaryOpNode::setKind(const QString &kind) {
        if (m_operator_kind == kind) return;
        m_operator_kind = kind;
        emit kindChanged();
    }

    void EditableBinaryOpNode::setLeft(EditableExpressionNode *node) {
        if (m_left.get() == node) return;
        if (node && node->parentNode() != nullptr) {
            qWarning() << "EditableBinaryOpNode::setLeft: node is already attached elsewhere; call "
                          "take*() on its current parent first";
            return;
        }
        if (node) {
            QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);
            node->setParent(nullptr);
        }
        m_left.reset(node);
        if (m_left) m_left->setParentNode(this);
        emit leftChanged();
    }

    void EditableBinaryOpNode::setRight(EditableExpressionNode *node) {
        if (m_right.get() == node) return;
        if (node && node->parentNode() != nullptr) {
            qWarning() << "EditableBinaryOpNode::setRight: node is already attached elsewhere; call "
                          "take*() on its current parent first";
            return;
        }
        if (node) {
            QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);
            node->setParent(nullptr);
        }
        m_right.reset(node);
        if (m_right) m_right->setParentNode(this);
        emit rightChanged();
    }

    std::unique_ptr<EditableExpressionNode> EditableBinaryOpNode::takeLeft() {
        if (m_left) m_left->setParentNode(nullptr);
        auto detached = std::move(m_left);
        emit leftChanged();
        return detached;
    }

    std::unique_ptr<EditableExpressionNode> EditableBinaryOpNode::takeRight() {
        if (m_right) m_right->setParentNode(nullptr);
        auto detached = std::move(m_right);
        emit rightChanged();
        return detached;
    }

    QString EditableBinaryOpNode::describe() const {
        static const QMap<QString, QString> symbols{
            {"add", "+"}, {"subtract", QString::fromUtf8("−")},
            {"multiply", QString::fromUtf8("×")}, {"divide", QString::fromUtf8("÷")}
        };
        const auto left_text = m_left ? m_left->describe() : QString::fromUtf8("…");
        const auto right_text = m_right ? m_right->describe() : QString::fromUtf8("…");
        return left_text + " " + symbols.value(m_operator_kind) + " " + right_text;
    }

    void EditableUnaryOpNode::setKind(const QString &kind) {
        if (m_operator_kind == kind) return;
        m_operator_kind = kind;
        emit kindChanged();
    }

    void EditableUnaryOpNode::setInput(EditableExpressionNode *node) {
        if (m_input.get() == node) return;
        if (node && node->parentNode() != nullptr) {
            qWarning() << "EditableUnaryOpNode::setInput: node is already attached elsewhere; call "
                          "take*() on its current parent first";
            return;
        }
        if (node) {
            QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);
            node->setParent(nullptr);
        }
        m_input.reset(node);
        if (m_input) m_input->setParentNode(this);
        emit inputChanged();
    }

    std::unique_ptr<EditableExpressionNode> EditableUnaryOpNode::takeInput() {
        if (m_input) m_input->setParentNode(nullptr);
        auto detached = std::move(m_input);
        emit inputChanged();
        return detached;
    }

    QString EditableUnaryOpNode::describe() const {
        static const QMap<QString, QString> labels{
            {"runningSum", "RunningSum"}, {"projectedFinalValue", "ProjectedFinalValue"},
            {"projectRateToFinal", "ProjectRateToFinal"}
        };
        const auto input_text = m_input ? m_input->describe() : QString::fromUtf8("…");
        return QString("%1(%2)").arg(labels.value(m_operator_kind), input_text);
    }

    void EditableRollingMeanNode::setWindow(uint32_t window) {
        if (m_window == window) return;
        m_window = window;
        emit windowChanged();
    }

    void EditableRollingMeanNode::setInput(EditableExpressionNode *node) {
        if (m_input.get() == node) return;
        if (node && node->parentNode() != nullptr) {
            qWarning() << "EditableRollingMeanNode::setInput: node is already attached elsewhere; "
                          "call take*() on its current parent first";
            return;
        }
        if (node) {
            QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);
            node->setParent(nullptr);
        }
        m_input.reset(node);
        if (m_input) m_input->setParentNode(this);
        emit inputChanged();
    }

    QString EditableRollingMeanNode::describe() const {
        const auto input_text = m_input ? m_input->describe() : QString::fromUtf8("…");
        return QString("RollingMean(window: %1, %2)").arg(m_window).arg(input_text);
    }

    std::unique_ptr<EditableExpressionNode> EditableRollingMeanNode::takeInput() {
        if (m_input) m_input->setParentNode(nullptr);
        auto detached = std::move(m_input);
        emit inputChanged();
        return detached;
    }

    void EditableAverageAcrossRunsNode::setSelectionKind(const QString &kind) {
        if (m_selection_kind == kind) return;
        m_selection_kind = kind;
        emit selectionKindChanged();
        if (kind == "recentRuns") setCount(5);
        else setPercent(10.0);
    }

    void EditableAverageAcrossRunsNode::setCount(uint32_t count) {
        if (m_count == count) return;
        m_count = count;
        emit countChanged();
    }

    void EditableAverageAcrossRunsNode::setPercent(double percent) {
        if (m_percent == percent) return;
        m_percent = percent;
        emit percentChanged();
    }

    void EditableAverageAcrossRunsNode::setInput(EditableExpressionNode *node) {
        if (m_input.get() == node) return;
        if (node && node->parentNode() != nullptr) {
            qWarning() << "EditableAverageAcrossRunsNode::setInput: node is already attached "
                          "elsewhere; call take*() on its current parent first";
            return;
        }
        if (node) {
            QQmlEngine::setObjectOwnership(node, QQmlEngine::CppOwnership);
            node->setParent(nullptr);
        }
        m_input.reset(node);
        if (m_input) m_input->setParentNode(this);
        emit inputChanged();
    }

    QString EditableAverageAcrossRunsNode::describe() const {
        const auto input_text = m_input ? m_input->describe() : QString::fromUtf8("…");
        const auto text = m_selection_kind == "recentRuns"
            ? QString("recent %1").arg(m_count)
            : QString("top %1%").arg(m_percent);
        return QString("AverageAcrossRuns(over: %1, %2)").arg(text, input_text);
    }

    std::unique_ptr<EditableExpressionNode> EditableAverageAcrossRunsNode::takeInput() {
        if (m_input) m_input->setParentNode(nullptr);
        auto detached = std::move(m_input);
        emit inputChanged();
        return detached;
    }
}
