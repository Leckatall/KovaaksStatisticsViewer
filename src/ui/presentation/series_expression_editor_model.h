#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>

#include "app/contracts/series_config.h"
#include "editable_expression_node.h"

namespace ksv::presentation {
    class SeriesExpressionEditorModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *root READ root NOTIFY rootChanged)
        Q_PROPERTY(ksv::presentation::EditableExpressionNode *selected READ selected NOTIFY selectedChanged)
        Q_PROPERTY(QStringList nodeKinds READ nodeKinds CONSTANT)
        Q_PROPERTY(QStringList primitiveMetrics READ primitiveMetrics CONSTANT)

    public:
        explicit SeriesExpressionEditorModel(QObject *parent = nullptr);
        ~SeriesExpressionEditorModel() override;

        [[nodiscard]] EditableExpressionNode *root() const { return m_root.get(); }
        [[nodiscard]] EditableExpressionNode *selected() const { return m_selected; }
        void loadFrom(const application::Expression &expression);

        Q_INVOKABLE void select(EditableExpressionNode *node);
        Q_INVOKABLE void replaceChild(EditableExpressionNode *parent, const QString &slot, const QString &kind);
        Q_INVOKABLE void deleteNode(EditableExpressionNode *node);
        Q_INVOKABLE void wrapSelected(const QString &kind);
        Q_INVOKABLE [[nodiscard]] QString describe(EditableExpressionNode *node) const;
        Q_INVOKABLE [[nodiscard]] bool isBinary(const QString &kind) const;
        Q_INVOKABLE [[nodiscard]] QStringList childSlotsFor(const QString &kind) const;
        Q_INVOKABLE [[nodiscard]] QVariantList ancestorChain(EditableExpressionNode *node) const;

        [[nodiscard]] QStringList nodeKinds() const;
        [[nodiscard]] QStringList primitiveMetrics() const;
        [[nodiscard]] std::optional<application::Expression> toExpression() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap toExpressionMap() const;

    signals:
        void rootChanged();
        void selectedChanged();

    private:
        void setRoot(EditableExpressionNode *node);
        void setSelected(EditableExpressionNode *node);
        [[nodiscard]] static EditableExpressionNode *makeNode(const QString &kind);
        [[nodiscard]] static QString slotOf(EditableExpressionNode *parent, EditableExpressionNode *child);
        [[nodiscard]] static bool setChildInParent(EditableExpressionNode *parent, const QString &slot,
                                                    EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> detachFromParent(EditableExpressionNode *node);
        [[nodiscard]] QVariantMap nodeToPersistenceMap(EditableExpressionNode *node) const;
        [[nodiscard]] EditableExpressionNode *nodeFromExpression(const application::Expression &expression);

        std::unique_ptr<EditableExpressionNode> m_root;
        EditableExpressionNode *m_selected = nullptr;
    };
}
