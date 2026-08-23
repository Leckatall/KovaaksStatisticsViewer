#pragma once

#include <QObject>
#include <QString>
#include <cstdint>
#include <memory>
#include <qqmlintegration.h>

namespace ksv::presentation {
    class EditableExpressionNode : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Abstract base, use a concrete node type")
        Q_PROPERTY(QString kind READ kind NOTIFY kindChanged)

    public:
        explicit EditableExpressionNode(QObject *parent = nullptr) : QObject(parent) {}
        ~EditableExpressionNode() override = default;

        [[nodiscard]] virtual QString kind() const = 0;
        [[nodiscard]] virtual QString describe() const = 0;

        [[nodiscard]] EditableExpressionNode *parentNode() const { return m_parent_node; }
        void setParentNode(EditableExpressionNode *parent) { m_parent_node = parent; }

    signals:
        void kindChanged();

    private:
        EditableExpressionNode *m_parent_node = nullptr;
    };

    class EditablePrimitiveNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QString metric READ metric WRITE setMetric NOTIFY metricChanged)

    public:
        explicit EditablePrimitiveNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return "primitive"; }
        [[nodiscard]] QString describe() const override { return m_metric; }
        [[nodiscard]] QString metric() const { return m_metric; }
        void setMetric(const QString &metric);

    signals:
        void metricChanged();

    private:
        QString m_metric = "score";
    };

    class EditableConstantNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)

    public:
        explicit EditableConstantNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return "constant"; }
        [[nodiscard]] QString describe() const override { return QString::number(m_value); }
        [[nodiscard]] double value() const { return m_value; }
        void setValue(double value);

    signals:
        void valueChanged();

    private:
        double m_value = 0.0;
    };

    class EditableBinaryOpNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QString operatorKind READ kind WRITE setKind NOTIFY kindChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *left READ left WRITE setLeft NOTIFY leftChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *right READ right WRITE setRight NOTIFY rightChanged)

    public:
        explicit EditableBinaryOpNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return m_operator_kind; }
        [[nodiscard]] QString describe() const override;
        void setKind(const QString &kind);
        [[nodiscard]] EditableExpressionNode *left() const { return m_left.get(); }
        void setLeft(EditableExpressionNode *node);
        [[nodiscard]] EditableExpressionNode *right() const { return m_right.get(); }
        void setRight(EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> takeLeft();
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> takeRight();

    signals:
        void leftChanged();
        void rightChanged();

    private:
        QString m_operator_kind = "add";
        std::unique_ptr<EditableExpressionNode> m_left;
        std::unique_ptr<EditableExpressionNode> m_right;
    };

    class EditableUnaryOpNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QString operatorKind READ kind WRITE setKind NOTIFY kindChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *input READ input WRITE setInput NOTIFY inputChanged)

    public:
        explicit EditableUnaryOpNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return m_operator_kind; }
        [[nodiscard]] QString describe() const override;
        void setKind(const QString &kind);
        [[nodiscard]] EditableExpressionNode *input() const { return m_input.get(); }
        void setInput(EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> takeInput();

    signals:
        void inputChanged();

    private:
        QString m_operator_kind = "runningSum";
        std::unique_ptr<EditableExpressionNode> m_input;
    };

    class EditableRollingMeanNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(uint32_t window READ window WRITE setWindow NOTIFY windowChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *input READ input WRITE setInput NOTIFY inputChanged)

    public:
        explicit EditableRollingMeanNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return "rollingMean"; }
        [[nodiscard]] QString describe() const override;
        [[nodiscard]] uint32_t window() const { return m_window; }
        void setWindow(uint32_t window);
        [[nodiscard]] EditableExpressionNode *input() const { return m_input.get(); }
        void setInput(EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> takeInput();

    signals:
        void windowChanged();
        void inputChanged();

    private:
        uint32_t m_window = 10;
        std::unique_ptr<EditableExpressionNode> m_input;
    };

    class EditableAverageAcrossRunsNode : public EditableExpressionNode {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QString selectionKind READ selectionKind WRITE setSelectionKind NOTIFY selectionKindChanged)
        Q_PROPERTY(uint32_t count READ count WRITE setCount NOTIFY countChanged)
        Q_PROPERTY(double percent READ percent WRITE setPercent NOTIFY percentChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *input READ input WRITE setInput NOTIFY inputChanged)

    public:
        explicit EditableAverageAcrossRunsNode(QObject *parent = nullptr) : EditableExpressionNode(parent) {}

        [[nodiscard]] QString kind() const override { return "averageAcrossRuns"; }
        [[nodiscard]] QString describe() const override;
        [[nodiscard]] QString selectionKind() const { return m_selection_kind; }
        void setSelectionKind(const QString &kind);
        [[nodiscard]] uint32_t count() const { return m_count; }
        void setCount(uint32_t count);
        [[nodiscard]] double percent() const { return m_percent; }
        void setPercent(double percent);
        [[nodiscard]] EditableExpressionNode *input() const { return m_input.get(); }
        void setInput(EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> takeInput();

    signals:
        void selectionKindChanged();
        void countChanged();
        void percentChanged();
        void inputChanged();

    private:
        QString m_selection_kind = "recentRuns";
        uint32_t m_count = 5;
        double m_percent = 0.0;
        std::unique_ptr<EditableExpressionNode> m_input;
    };
}
