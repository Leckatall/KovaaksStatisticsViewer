#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_BREAKDOWN_MODEL_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_BREAKDOWN_MODEL_H

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <qqmlintegration.h>

#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_projection.h"

namespace ksv::presentation {
    // Read-only Uncategorized/category/subcategory/scenario tree rebuilt from the selected benchmark
    // definition joined with its projection. Node keys are stable benchmark element IDs. The model
    // owns the expansion state that BenchmarkTrackingViewModel::setExpanded drives: groups and
    // subcategories start expanded, expansion survives a same-benchmark projection refresh keyed by
    // group ID, and it resets to those defaults when the selected benchmark ID changes.
    class BenchmarkBreakdownModel final : public QAbstractItemModel {
        Q_OBJECT
        // Nested value-tree mirror of the model, keyed the same way the `blockers` and
        // `selectorEntries` view-model projections are, for QML that walks children/expanded
        // directly rather than through a QAbstractItemView.
        Q_PROPERTY(QVariantList nodes READ nodes NOTIFY nodesChanged)

    public:
        enum Roles {
            NodeIdRole = Qt::UserRole + 1,
            KindRole,
            NameRole,
            ColorRole,
            HighestTierTextRole,
            ChildCountRole,
            NextTierBlockerRole,
            PersonalBestTextRole,
            RecentAverageTextRole,
            RecentSampleCountRole,
            AttainedTierTextRole,
            NextThresholdTextRole,
            MatchStateTextRole,
            ExpandedRole,
        };

        explicit BenchmarkBreakdownModel(QObject *parent = nullptr);

        [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex &parent) const override;
        [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
        [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
        [[nodiscard]] int columnCount(const QModelIndex &parent) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] QVariantList nodes() const { return m_nodesTree; }

        // Rebuilds the tree for one coherent revision. `definition`/`projection` are null outside
        // the Ready states, which yields an empty model.
        void reset(const domain::Benchmark *definition, const domain::BenchmarkProjection *projection);

        void setExpanded(const QString &nodeId, bool expanded);

    signals:
        void nodesChanged();

    private:
        struct Node {
            QString nodeId;
            QString kind;
            QString name;
            QColor color;
            QString highestTierText;
            bool nextTierBlocker = false;
            QString personalBestText;
            QString recentAverageText;
            int recentSampleCount = 0;
            QString attainedTierText;
            QString nextThresholdText;
            QString matchStateText;
            int parent = -1;
            int row = 0;
            std::vector<int> children;
        };

        int pushNode(Node node, int parent);
        void buildTree(const domain::Benchmark &definition, const domain::BenchmarkProjection &projection);
        void addScenario(int parentIndex, const domain::ScenarioEntry &entry,
                         const QHash<QString, const domain::ScenarioProjection *> &scenarios,
                         const QHash<QString, QString> &tierNames, const QSet<QString> &blockedEntries);
        bool propagateBlocker(int index);
        void rebuildNodeTree();
        [[nodiscard]] QVariantList nodeTreeFor(const std::vector<int> &indices) const;

        std::vector<Node> m_nodes;
        std::vector<int> m_roots;
        QHash<QString, int> m_indexByNodeId;
        QHash<QString, bool> m_expanded;
        QString m_benchmarkId;
        QVariantList m_nodesTree;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_BREAKDOWN_MODEL_H
