#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TREE_NODE_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TREE_NODE_H

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>
#include <qqmlintegration.h>
#include <vector>

namespace ksv::presentation {
    // Read-only display node for the manager's rebuilt hierarchy tree. The VM owns and
    // rebuilds the whole tree on every draft change; all edits route back through the
    // service, so these properties never need WRITE accessors.
    class BenchmarkTreeNode : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Built by BenchmarkManagerViewModel")
        Q_PROPERTY(QString nodeId READ nodeId CONSTANT)
        Q_PROPERTY(QString kind READ kind CONSTANT) // "uncategorized"|"category"|"subcategory"|"scenario"
        Q_PROPERTY(QString name READ name CONSTANT)

    public:
        BenchmarkTreeNode(QString nodeId, QString kind, QString name, QObject *parent = nullptr);
        ~BenchmarkTreeNode() override = default;

        [[nodiscard]] const QString &nodeId() const { return m_nodeId; }
        [[nodiscard]] const QString &kind() const { return m_kind; }
        [[nodiscard]] const QString &name() const { return m_name; }

        void appendChild(std::unique_ptr<BenchmarkTreeNode> child);
        [[nodiscard]] const std::vector<std::unique_ptr<BenchmarkTreeNode>> &childNodes() const {
            return m_children;
        }

    private:
        QString m_nodeId;
        QString m_kind;
        QString m_name;
        std::vector<std::unique_ptr<BenchmarkTreeNode>> m_children;
    };

    class BenchmarkGroupNode : public BenchmarkTreeNode {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Built by BenchmarkManagerViewModel")
        Q_PROPERTY(QColor color READ color CONSTANT)
        Q_PROPERTY(QVariantList children READ children CONSTANT)

    public:
        BenchmarkGroupNode(QString nodeId, QString kind, QString name, QColor color,
                           QObject *parent = nullptr);

        [[nodiscard]] const QColor &color() const { return m_color; }
        [[nodiscard]] QVariantList children() const;

    private:
        QColor m_color;
    };

    class BenchmarkScenarioNode : public BenchmarkTreeNode {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Built by BenchmarkManagerViewModel")
        Q_PROPERTY(bool hasHash READ hasHash CONSTANT)
        // One entry per tier in ladder order; hasValue distinguishes an entered threshold
        // from the empty slot the editor shows for it.
        Q_PROPERTY(QVariantList thresholds READ thresholds CONSTANT)

    public:
        BenchmarkScenarioNode(QString nodeId, QString name, bool hasHash, QVariantList thresholds,
                              QObject *parent = nullptr);

        [[nodiscard]] bool hasHash() const { return m_hasHash; }
        [[nodiscard]] const QVariantList &thresholds() const { return m_thresholds; }

    private:
        bool m_hasHash;
        QVariantList m_thresholds;
    };
}

#endif
