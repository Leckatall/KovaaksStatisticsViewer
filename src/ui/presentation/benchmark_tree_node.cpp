#include "benchmark_tree_node.h"

namespace ksv::presentation {
    BenchmarkTreeNode::BenchmarkTreeNode(QString nodeId, QString kind, QString name, QObject *parent)
        : QObject(parent), m_nodeId(std::move(nodeId)), m_kind(std::move(kind)), m_name(std::move(name)) {}

    void BenchmarkTreeNode::appendChild(std::unique_ptr<BenchmarkTreeNode> child) {
        m_children.push_back(std::move(child));
    }

    BenchmarkGroupNode::BenchmarkGroupNode(QString nodeId, QString kind, QString name, QColor color,
                                           QObject *parent)
        : BenchmarkTreeNode(std::move(nodeId), std::move(kind), std::move(name), parent),
          m_color(std::move(color)) {}

    QVariantList BenchmarkGroupNode::children() const {
        QVariantList list;
        list.reserve(static_cast<qsizetype>(childNodes().size()));
        for (const auto &child: childNodes()) list.push_back(QVariant::fromValue(child.get()));
        return list;
    }

    BenchmarkScenarioNode::BenchmarkScenarioNode(QString nodeId, QString name, bool hasHash,
                                                 QVariantList thresholds, QObject *parent)
        : BenchmarkTreeNode(std::move(nodeId), QStringLiteral("scenario"), std::move(name), parent),
          m_hasHash(hasHash),
          m_thresholds(std::move(thresholds)) {}
}
