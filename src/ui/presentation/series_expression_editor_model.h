#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <memory>

#include "app/contracts/series_config.h"

namespace ksv::presentation {
    class SeriesExpressionEditorModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QVariantMap root READ root NOTIFY treeChanged)
        Q_PROPERTY(QString selectedNodeId READ selectedNodeId NOTIFY treeChanged)
        Q_PROPERTY(int treeRevision READ treeRevision NOTIFY treeChanged)
        Q_PROPERTY(QStringList nodeKinds READ nodeKinds CONSTANT)
        Q_PROPERTY(QStringList primitiveMetrics READ primitiveMetrics CONSTANT)

    public:
        explicit SeriesExpressionEditorModel(QObject *parent = nullptr);
        ~SeriesExpressionEditorModel() override;

        [[nodiscard]] QVariantMap root() const;
        [[nodiscard]] QString selectedNodeId() const;
        [[nodiscard]] int treeRevision() const;
        void loadFrom(const application::Expression &expression);

        Q_INVOKABLE void select(const QString &id);
        Q_INVOKABLE void replaceChild(const QString &parentId, const QString &slot, const QString &kind);
        Q_INVOKABLE void deleteNode(const QString &id);
        Q_INVOKABLE void wrapSelected(const QString &kind);
        Q_INVOKABLE void changeBinaryOperator(const QString &id, const QString &kind);
        Q_INVOKABLE void updateField(const QString &id, const QString &field, const QVariant &value);
        Q_INVOKABLE void changeSelectionKind(const QString &id, const QString &kind);
        Q_INVOKABLE [[nodiscard]] QString describe(const QVariantMap &node) const;
        Q_INVOKABLE [[nodiscard]] bool isBinary(const QString &kind) const;
        Q_INVOKABLE [[nodiscard]] QStringList childSlotsFor(const QString &kind) const;
        Q_INVOKABLE [[nodiscard]] QVariantList ancestorChain(const QString &id) const;

        [[nodiscard]] QStringList nodeKinds() const;
        [[nodiscard]] QStringList primitiveMetrics() const;
        [[nodiscard]] std::optional<application::Expression> toExpression() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap toExpressionMap() const;

    signals:
        void treeChanged();

    private:
        struct MutableExprNode;

        [[nodiscard]] std::shared_ptr<MutableExprNode> makeNode(const QString &kind);
        [[nodiscard]] std::shared_ptr<MutableExprNode> findNode(const QString &id) const;
        [[nodiscard]] QVariantMap nodeToEditableMap(const std::shared_ptr<MutableExprNode> &node) const;
        [[nodiscard]] QVariantMap nodeToPersistenceMap(const std::shared_ptr<MutableExprNode> &node) const;
        [[nodiscard]] std::shared_ptr<MutableExprNode> nodeFromExpression(const application::Expression &expression);
        [[nodiscard]] bool findLocation(const QString &id, const std::shared_ptr<MutableExprNode> &node,
                                        std::shared_ptr<MutableExprNode> *parent, QString *slot) const;
        [[nodiscard]] bool pathTo(const QString &id, const std::shared_ptr<MutableExprNode> &node,
                                  QList<std::shared_ptr<MutableExprNode>> &path) const;
        void touch();

        std::shared_ptr<MutableExprNode> m_root;
        QString m_selected_node_id;
        int m_tree_revision = 0;
        int m_next_id = 1;
    };
}
