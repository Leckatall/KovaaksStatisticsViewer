#include "benchmark_breakdown_model.h"

#include <QVariant>
#include <utility>

#include "benchmark_format.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"

namespace ksv::presentation {
    namespace {
        QString q(const std::string &value) { return QString::fromStdString(value); }

        QString matchStateLabel(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved:
                    return QObject::tr("Resolved");
                case domain::ScenarioMatchState::MappedUnavailable:
                    return QObject::tr("Mapped, unavailable");
                case domain::ScenarioMatchState::Unresolved:
                    return QObject::tr("Unresolved");
                case domain::ScenarioMatchState::Ambiguous:
                    return QObject::tr("Ambiguous");
                case domain::ScenarioMatchState::AutoMappable:
                    return QObject::tr("Pending automatic mapping");
            }
            return {};
        }

        void indexGroups(const std::vector<domain::GroupProjection> &groups,
                         QHash<QString, const domain::GroupProjection *> &out) {
            for (const auto &group: groups) {
                out.insert(q(group.id.value), &group);
                indexGroups(group.subgroups, out);
            }
        }
    }

    BenchmarkBreakdownModel::BenchmarkBreakdownModel(QObject *parent) : QAbstractItemModel(parent) {}

    QModelIndex BenchmarkBreakdownModel::index(const int row, const int column, const QModelIndex &parent) const {
        if (!hasIndex(row, column, parent)) return {};
        const auto parentIndex = static_cast<std::size_t>(parent.internalId());
        if (parent.isValid() && parentIndex >= m_nodes.size()) return {};
        const std::vector<int> &siblings = parent.isValid() ? m_nodes[parentIndex].children : m_roots;
        if (row < 0 || row >= static_cast<int>(siblings.size())) return {};
        return createIndex(row, column, static_cast<quintptr>(siblings[static_cast<std::size_t>(row)]));
    }

    QModelIndex BenchmarkBreakdownModel::parent(const QModelIndex &child) const {
        if (!child.isValid()) return {};
        const auto childIndex = static_cast<std::size_t>(child.internalId());
        if (childIndex >= m_nodes.size()) return {};
        const Node &node = m_nodes[childIndex];
        if (node.parent < 0 || static_cast<std::size_t>(node.parent) >= m_nodes.size()) return {};
        const Node &grandparent = m_nodes[static_cast<std::size_t>(node.parent)];
        return createIndex(grandparent.row, 0, static_cast<quintptr>(node.parent));
    }

    int BenchmarkBreakdownModel::rowCount(const QModelIndex &parent) const {
        if (parent.column() > 0) return 0;
        if (!parent.isValid()) return static_cast<int>(m_roots.size());
        const auto parentIndex = static_cast<std::size_t>(parent.internalId());
        if (parentIndex >= m_nodes.size()) return 0;
        return static_cast<int>(m_nodes[parentIndex].children.size());
    }

    int BenchmarkBreakdownModel::columnCount(const QModelIndex &) const { return 1; }

    QVariant BenchmarkBreakdownModel::data(const QModelIndex &index, const int role) const {
        if (!index.isValid()) return {};
        const auto nodeIndex = static_cast<std::size_t>(index.internalId());
        if (nodeIndex >= m_nodes.size()) return {};
        const Node &node = m_nodes[nodeIndex];
        switch (role) {
            case NodeIdRole: return node.nodeId;
            case KindRole: return node.kind;
            case NameRole: return node.name;
            case ColorRole: return QVariant::fromValue(node.color);
            case HighestTierTextRole: return node.highestTierText;
            case ChildCountRole: return static_cast<int>(node.children.size());
            case NextTierBlockerRole: return node.nextTierBlocker;
            case PersonalBestTextRole: return node.personalBestText;
            case RecentAverageTextRole: return node.recentAverageText;
            case RecentSampleCountRole: return node.recentSampleCount;
            case AttainedTierTextRole: return node.attainedTierText;
            case NextThresholdTextRole: return node.nextThresholdText;
            case MatchStateTextRole: return node.matchStateText;
            case ExpandedRole: return m_expanded.value(node.nodeId, true);
            default: return {};
        }
    }

    Qt::ItemFlags BenchmarkBreakdownModel::flags(const QModelIndex &index) const {
        if (!index.isValid()) return Qt::NoItemFlags;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    QHash<int, QByteArray> BenchmarkBreakdownModel::roleNames() const {
        return {
            {NodeIdRole, "nodeId"},
            {KindRole, "kind"},
            {NameRole, "name"},
            {ColorRole, "color"},
            {HighestTierTextRole, "highestTierText"},
            {ChildCountRole, "childCount"},
            {NextTierBlockerRole, "nextTierBlocker"},
            {PersonalBestTextRole, "personalBestText"},
            {RecentAverageTextRole, "recentAverageText"},
            {RecentSampleCountRole, "recentSampleCount"},
            {AttainedTierTextRole, "attainedTierText"},
            {NextThresholdTextRole, "nextThresholdText"},
            {MatchStateTextRole, "matchStateText"},
            {ExpandedRole, "expanded"},
        };
    }

    void BenchmarkBreakdownModel::reset(const domain::Benchmark *definition,
                                        const domain::BenchmarkProjection *projection) {
        beginResetModel();
        m_nodes.clear();
        m_roots.clear();
        m_indexByNodeId.clear();

        const QString newBenchmarkId = definition ? q(definition->id.value) : QString();
        if (newBenchmarkId != m_benchmarkId) {
            m_expanded.clear();
            m_benchmarkId = newBenchmarkId;
        }

        if (definition && projection) buildTree(*definition, *projection);
        rebuildNodeTree();
        endResetModel();
        emit nodesChanged();
    }

    void BenchmarkBreakdownModel::setExpanded(const QString &nodeId, const bool expanded) {
        m_expanded[nodeId] = expanded;
        rebuildNodeTree();
        emit nodesChanged();
        const auto found = m_indexByNodeId.constFind(nodeId);
        if (found == m_indexByNodeId.constEnd()) return;
        const Node &node = m_nodes[static_cast<std::size_t>(found.value())];
        const QModelIndex changed = createIndex(node.row, 0, static_cast<quintptr>(found.value()));
        emit dataChanged(changed, changed, {ExpandedRole});
    }

    void BenchmarkBreakdownModel::rebuildNodeTree() { m_nodesTree = nodeTreeFor(m_roots); }

    QVariantList BenchmarkBreakdownModel::nodeTreeFor(const std::vector<int> &indices) const {
        QVariantList list;
        for (const int idx: indices) {
            const Node &node = m_nodes[static_cast<std::size_t>(idx)];
            QVariantMap map;
            map[QStringLiteral("nodeId")] = node.nodeId;
            map[QStringLiteral("kind")] = node.kind;
            map[QStringLiteral("name")] = node.name;
            map[QStringLiteral("color")] = QVariant::fromValue(node.color);
            map[QStringLiteral("highestTierText")] = node.highestTierText;
            map[QStringLiteral("childCount")] = static_cast<int>(node.children.size());
            map[QStringLiteral("nextTierBlocker")] = node.nextTierBlocker;
            map[QStringLiteral("personalBestText")] = node.personalBestText;
            map[QStringLiteral("recentAverageText")] = node.recentAverageText;
            map[QStringLiteral("recentSampleCount")] = node.recentSampleCount;
            map[QStringLiteral("attainedTierText")] = node.attainedTierText;
            map[QStringLiteral("nextThresholdText")] = node.nextThresholdText;
            map[QStringLiteral("matchStateText")] = node.matchStateText;
            map[QStringLiteral("expanded")] = m_expanded.value(node.nodeId, true);
            map[QStringLiteral("children")] = nodeTreeFor(node.children);
            list.append(map);
        }
        return list;
    }

    int BenchmarkBreakdownModel::pushNode(Node node, const int parent) {
        node.parent = parent;
        const int nodeIndex = static_cast<int>(m_nodes.size());
        node.row = parent < 0 ? static_cast<int>(m_roots.size())
                              : static_cast<int>(m_nodes[static_cast<std::size_t>(parent)].children.size());
        const QString nodeId = node.nodeId;
        m_nodes.push_back(std::move(node));
        if (parent < 0) m_roots.push_back(nodeIndex);
        else m_nodes[static_cast<std::size_t>(parent)].children.push_back(nodeIndex);
        if (!nodeId.isEmpty()) m_indexByNodeId.insert(nodeId, nodeIndex);
        return nodeIndex;
    }

    void BenchmarkBreakdownModel::addScenario(
        const int parentIndex, const domain::ScenarioEntry &entry,
        const QHash<QString, const domain::ScenarioProjection *> &scenarios,
        const QHash<QString, QString> &tierNames, const QSet<QString> &blockedEntries) {
        Node node;
        node.nodeId = q(entry.id.value);
        node.kind = QStringLiteral("scenario");

        const domain::ScenarioProjection *projected = scenarios.value(node.nodeId, nullptr);
        node.name = projected ? q(projected->name) : q(entry.name);
        if (projected) {
            node.personalBestText =
                projected->personalBest ? formatBenchmarkNumber(*projected->personalBest) : tr("Unplayed");
            node.recentAverageText =
                projected->recentAverage ? formatBenchmarkNumber(*projected->recentAverage) : tr("No completed runs");
            node.recentSampleCount =
                (projected->recentSampleCount >= 1 && projected->recentSampleCount <= 4)
                    ? projected->recentSampleCount
                    : 0;
            node.attainedTierText = projected->attainedTier
                                        ? tierNames.value(q(projected->attainedTier->value), tr("Unranked"))
                                        : tr("Unranked");
            node.nextThresholdText = projected->nextThreshold
                                         ? formatBenchmarkNumber(projected->nextThreshold->score)
                                         : tr("Highest tier attained");
            node.matchStateText = matchStateLabel(projected->matchState);
        } else {
            node.personalBestText = tr("Unplayed");
            node.recentAverageText = tr("No completed runs");
            node.attainedTierText = tr("Unranked");
            node.nextThresholdText = tr("Highest tier attained");
            node.matchStateText = matchStateLabel(domain::ScenarioMatchState::Unresolved);
        }
        node.nextTierBlocker = blockedEntries.contains(node.nodeId);
        pushNode(std::move(node), parentIndex);
    }

    void BenchmarkBreakdownModel::buildTree(const domain::Benchmark &definition,
                                            const domain::BenchmarkProjection &projection) {
        QHash<QString, QString> tierNames;
        for (const auto &tier: projection.tiers) tierNames.insert(q(tier.id.value), q(tier.name));

        QHash<QString, const domain::ScenarioProjection *> scenarios;
        for (const auto &scenario: projection.scenarios) scenarios.insert(q(scenario.entryId.value), &scenario);

        QHash<QString, const domain::GroupProjection *> groups;
        indexGroups(projection.categories, groups);

        QSet<QString> blockedEntries;
        QSet<QString> namedGroups;
        for (const auto &blocker: projection.nextTierBlockers) {
            blockedEntries.insert(q(blocker.entryId.value));
            if (blocker.group) namedGroups.insert(q(blocker.group->value));
        }

        const auto highestTierText = [&](const QString &groupId) -> QString {
            const domain::GroupProjection *group = groups.value(groupId, nullptr);
            if (group && group->satisfiedTier) return tierNames.value(q(group->satisfiedTier->value), tr("Unranked"));
            return tr("Unranked");
        };

        Node uncategorized;
        uncategorized.kind = QStringLiteral("uncategorized");
        uncategorized.name = tr("Uncategorized");
        const int uncategorizedIndex = pushNode(std::move(uncategorized), -1);
        for (const auto &entry: definition.uncategorized)
            addScenario(uncategorizedIndex, entry, scenarios, tierNames, blockedEntries);

        for (const auto &category: definition.categories) {
            Node categoryNode;
            categoryNode.nodeId = q(category.id.value);
            categoryNode.kind = QStringLiteral("category");
            categoryNode.name = q(category.name);
            categoryNode.color = toQColor(category.color);
            categoryNode.highestTierText = highestTierText(categoryNode.nodeId);
            const int categoryIndex = pushNode(std::move(categoryNode), -1);

            for (const auto &entry: category.scenarios)
                addScenario(categoryIndex, entry, scenarios, tierNames, blockedEntries);

            for (const auto &subcategory: category.subcategories) {
                Node subNode;
                subNode.nodeId = q(subcategory.id.value);
                subNode.kind = QStringLiteral("subcategory");
                subNode.name = q(subcategory.name);
                subNode.color = toQColor(subcategory.color);
                subNode.highestTierText = highestTierText(subNode.nodeId);
                const int subIndex = pushNode(std::move(subNode), categoryIndex);
                for (const auto &entry: subcategory.scenarios)
                    addScenario(subIndex, entry, scenarios, tierNames, blockedEntries);
            }
        }

        for (const QString &groupId: namedGroups) {
            const auto found = m_indexByNodeId.constFind(groupId);
            if (found != m_indexByNodeId.constEnd())
                m_nodes[static_cast<std::size_t>(found.value())].nextTierBlocker = true;
        }
        for (const int root: m_roots) propagateBlocker(root);
    }

    bool BenchmarkBreakdownModel::propagateBlocker(const int index) {
        bool anyChild = false;
        for (const int child: m_nodes[static_cast<std::size_t>(index)].children)
            if (propagateBlocker(child)) anyChild = true;
        Node &node = m_nodes[static_cast<std::size_t>(index)];
        if (anyChild && node.kind != QLatin1String("scenario")) node.nextTierBlocker = true;
        return node.nextTierBlocker;
    }
}
