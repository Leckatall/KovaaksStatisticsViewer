#include "benchmark_breakdown_model.h"

#include <QVariant>
#include <utility>

#include "benchmark_format.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"

namespace ksv::presentation {
    namespace {
        QString q(const std::string &value) { return QString::fromStdString(value); }

        // Tier colours travel as "#rrggbb" or "" so QML can tell "no tier colour" apart; an invalid
        // QColor reaches QML as opaque black.
        QString colorText(const QColor &color) { return color.isValid() ? color.name() : QString(); }

        // A normally resolved scenario reports nothing; the user only sees match states they may
        // need to act on, in their own terms rather than the resolver's.
        QString matchStateLabel(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved:
                    return {};
                case domain::ScenarioMatchState::MappedUnavailable:
                    return QObject::tr("No runs of linked scenario");
                case domain::ScenarioMatchState::Unresolved:
                    return QObject::tr("Not found in your runs");
                case domain::ScenarioMatchState::Ambiguous:
                    return QObject::tr("Several scenarios match");
                case domain::ScenarioMatchState::AutoMappable:
                    return QObject::tr("Linking to your runs…");
            }
            return {};
        }

        QString matchStateHint(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved:
                    return {};
                case domain::ScenarioMatchState::MappedUnavailable:
                    return QObject::tr("This entry is linked to a scenario that has no runs in your profile. "
                                       "Play it in KovaaK's, or link the entry to another scenario in the "
                                       "Benchmark Manager.");
                case domain::ScenarioMatchState::Unresolved:
                    return QObject::tr("No scenario in your runs has this name. Play it in KovaaK's, or link "
                                       "the entry to a scenario in the Benchmark Manager.");
                case domain::ScenarioMatchState::Ambiguous:
                    return QObject::tr("More than one scenario in your runs has this name. Choose the right "
                                       "one in the Benchmark Manager.");
                case domain::ScenarioMatchState::AutoMappable:
                    return QObject::tr("A matching scenario was found in your runs and is being linked "
                                       "automatically.");
            }
            return {};
        }

        int scenarioCountOf(const domain::Subcategory &subcategory) {
            return static_cast<int>(subcategory.scenarios.size());
        }

        int scenarioCountOf(const domain::Category &category) {
            int count = static_cast<int>(category.scenarios.size());
            for (const auto &subcategory: category.subcategories) count += scenarioCountOf(subcategory);
            return count;
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
            case ScenarioCountRole: return node.scenarioCount;
            case HighestTierColorRole: return colorText(node.highestTierColor);
            case AttainedTierColorRole: return colorText(node.attainedTierColor);
            case NextTierTextRole: return node.nextTierText;
            case NextTierColorRole: return colorText(node.nextTierColor);
            case MatchStateHintRole: return node.matchStateHint;
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
            {ScenarioCountRole, "scenarioCount"},
            {HighestTierColorRole, "highestTierColor"},
            {AttainedTierColorRole, "attainedTierColor"},
            {NextTierTextRole, "nextTierText"},
            {NextTierColorRole, "nextTierColor"},
            {MatchStateHintRole, "matchStateHint"},
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
            map[QStringLiteral("matchStateHint")] = node.matchStateHint;
            map[QStringLiteral("scenarioCount")] = node.scenarioCount;
            map[QStringLiteral("highestTierColor")] = colorText(node.highestTierColor);
            map[QStringLiteral("attainedTierColor")] = colorText(node.attainedTierColor);
            map[QStringLiteral("nextTierText")] = node.nextTierText;
            map[QStringLiteral("nextTierColor")] = colorText(node.nextTierColor);
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
        const QHash<QString, TierStyle> &tiers, const QSet<QString> &blockedEntries) {
        Node node;
        node.nodeId = q(entry.id.value);
        node.kind = QStringLiteral("scenario");
        node.scenarioCount = 1;

        const domain::ScenarioProjection *projected = scenarios.value(node.nodeId, nullptr);
        node.name = projected ? q(projected->name) : q(entry.name);
        node.attainedTierText = tr("Unranked");
        node.nextThresholdText = tr("Highest tier attained");
        if (projected) {
            node.personalBestText =
                projected->personalBest ? formatBenchmarkNumber(*projected->personalBest) : tr("Unplayed");
            node.recentAverageText =
                projected->recentAverage ? formatBenchmarkNumber(*projected->recentAverage) : tr("No completed runs");
            node.recentSampleCount =
                (projected->recentSampleCount >= 1 && projected->recentSampleCount <= 4)
                    ? projected->recentSampleCount
                    : 0;
            if (projected->attainedTier) {
                const auto found = tiers.constFind(q(projected->attainedTier->value));
                if (found != tiers.constEnd()) {
                    node.attainedTierText = found->name;
                    node.attainedTierColor = found->color;
                }
            }
            if (projected->nextThreshold) {
                node.nextThresholdText = formatBenchmarkNumber(projected->nextThreshold->score);
                const auto found = tiers.constFind(q(projected->nextThreshold->tierId.value));
                if (found != tiers.constEnd()) {
                    node.nextTierText = found->name;
                    node.nextTierColor = found->color;
                }
            }
            node.matchStateText = matchStateLabel(projected->matchState);
            node.matchStateHint = matchStateHint(projected->matchState);
        } else {
            node.personalBestText = tr("Unplayed");
            node.recentAverageText = tr("No completed runs");
            node.matchStateText = matchStateLabel(domain::ScenarioMatchState::Unresolved);
            node.matchStateHint = matchStateHint(domain::ScenarioMatchState::Unresolved);
        }
        node.nextTierBlocker = blockedEntries.contains(node.nodeId);
        pushNode(std::move(node), parentIndex);
    }

    void BenchmarkBreakdownModel::buildTree(const domain::Benchmark &definition,
                                            const domain::BenchmarkProjection &projection) {
        QHash<QString, TierStyle> tiers;
        for (const auto &tier: projection.tiers)
            tiers.insert(q(tier.id.value), TierStyle{q(tier.name), readableTextColor(tier.color)});

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

        const auto applyHighestTier = [&](Node &node) {
            node.highestTierText = tr("Unranked");
            const domain::GroupProjection *group = groups.value(node.nodeId, nullptr);
            if (!group || !group->satisfiedTier) return;
            const auto found = tiers.constFind(q(group->satisfiedTier->value));
            if (found == tiers.constEnd()) return;
            node.highestTierText = found->name;
            node.highestTierColor = found->color;
        };

        // Empty groups carry no information (and an empty Uncategorized bucket is the normal case
        // for a fully categorised benchmark), so only groups that hold scenarios become nodes.
        if (!definition.uncategorized.empty()) {
            Node uncategorized;
            uncategorized.kind = QStringLiteral("uncategorized");
            uncategorized.name = tr("Uncategorized");
            uncategorized.scenarioCount = static_cast<int>(definition.uncategorized.size());
            const int uncategorizedIndex = pushNode(std::move(uncategorized), -1);
            for (const auto &entry: definition.uncategorized)
                addScenario(uncategorizedIndex, entry, scenarios, tiers, blockedEntries);
        }

        for (const auto &category: definition.categories) {
            const int categoryScenarios = scenarioCountOf(category);
            if (categoryScenarios == 0) continue;
            Node categoryNode;
            categoryNode.nodeId = q(category.id.value);
            categoryNode.kind = QStringLiteral("category");
            categoryNode.name = q(category.name);
            categoryNode.color = toQColor(category.color);
            categoryNode.scenarioCount = categoryScenarios;
            applyHighestTier(categoryNode);
            const int categoryIndex = pushNode(std::move(categoryNode), -1);

            for (const auto &entry: category.scenarios)
                addScenario(categoryIndex, entry, scenarios, tiers, blockedEntries);

            for (const auto &subcategory: category.subcategories) {
                const int subcategoryScenarios = scenarioCountOf(subcategory);
                if (subcategoryScenarios == 0) continue;
                Node subNode;
                subNode.nodeId = q(subcategory.id.value);
                subNode.kind = QStringLiteral("subcategory");
                subNode.name = q(subcategory.name);
                subNode.color = toQColor(subcategory.color);
                subNode.scenarioCount = subcategoryScenarios;
                applyHighestTier(subNode);
                const int subIndex = pushNode(std::move(subNode), categoryIndex);
                for (const auto &entry: subcategory.scenarios)
                    addScenario(subIndex, entry, scenarios, tiers, blockedEntries);
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
