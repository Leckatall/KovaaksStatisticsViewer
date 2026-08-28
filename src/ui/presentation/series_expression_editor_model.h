#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

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
        Q_PROPERTY(QString dslText READ toDslText NOTIFY dslTextChanged)

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

        // Canonical DSL text of the current expression, empty when it is incomplete/invalid.
        Q_INVOKABLE [[nodiscard]] QString toDslText() const;
        // Replaces the tree from DSL text; returns false and leaves the tree untouched when text is invalid.
        Q_INVOKABLE bool applyDslText(const QString &text);
        // Clipboard glue over the two methods above, for the editor dialog's copy/paste controls.
        Q_INVOKABLE void copyToClipboard() const;
        Q_INVOKABLE bool pasteFromClipboard();

    signals:
        void rootChanged();
        void selectedChanged();
        void dslTextChanged();

    private:
        void setRoot(EditableExpressionNode *node);
        // Chains every node's property-change signals to dslTextChanged so a live DSL view reflects
        // field edits (which write node properties directly), then emits once for the structural change.
        void refreshDslObservers();
        void observeSubtree(EditableExpressionNode *node);
        void setSelected(EditableExpressionNode *node);
        [[nodiscard]] static EditableExpressionNode *makeNode(const QString &kind);
        [[nodiscard]] static QString slotOf(EditableExpressionNode *parent, EditableExpressionNode *child);
        static bool setChildInParent(EditableExpressionNode *parent, const QString &slot,
                                                    EditableExpressionNode *node);
        [[nodiscard]] std::unique_ptr<EditableExpressionNode> detachFromParent(EditableExpressionNode *node);
        [[nodiscard]] std::optional<application::Expression> nodeToExpression(EditableExpressionNode *node) const;
        [[nodiscard]] EditableExpressionNode *nodeFromExpression(const application::Expression &expression);

        std::unique_ptr<EditableExpressionNode> m_root;
        EditableExpressionNode *m_selected = nullptr;
    };
}
