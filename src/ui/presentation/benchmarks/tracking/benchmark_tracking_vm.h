#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <qqmlintegration.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "app/contracts/benchmark_workspace_snapshot.h"
#include "app/contracts/i_benchmark_tracking_use_case.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"
#include "presentation/graphs/common/axes/date_time_axis.h"

namespace ksv::presentation {
    // Adapts one IBenchmarkTrackingUseCase revision into the benchmark workspace's QML surface:
    // selector entries, explicit state, status summary, and three owned presentation-only child
    // models (two history graphs and the scenario progress grid). Its only dependency is the
    // use case - never a service, profile, or repository. After an onChanged notification it reads
    // one BenchmarkWorkspaceSnapshot, builds every replacement value plus all child models, installs
    // them, and only then emits changed(), so QML never sees a new state over a stale child model.
    class BenchmarkTrackingViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString state READ state NOTIFY changed)
        Q_PROPERTY(QString stateMessage READ stateMessage NOTIFY changed)
        Q_PROPERTY(QString selectedName READ selectedName NOTIFY changed)
        Q_PROPERTY(QVariantList selectorEntries READ selectorEntries NOTIFY changed)
        Q_PROPERTY(QStringList completenessIssues READ completenessIssues NOTIFY changed)
        Q_PROPERTY(bool summaryAvailable READ summaryAvailable NOTIFY changed)
        Q_PROPERTY(QString attainedRank READ attainedRank NOTIFY changed)
        Q_PROPERTY(QString completedRank READ completedRank NOTIFY changed)
        Q_PROPERTY(QString nextTier READ nextTier NOTIFY changed)
        // Readable text colours ("#rrggbb") of the tiers named by attainedRank/completedRank/
        // nextTier, or "" when the value is not a tier (Unranked, None completed, ...).
        Q_PROPERTY(QString attainedRankColor READ attainedRankColor NOTIFY changed)
        Q_PROPERTY(QString completedRankColor READ completedRankColor NOTIFY changed)
        Q_PROPERTY(QString nextTierColor READ nextTierColor NOTIFY changed)
        // The average rank in words ("Bronze · 60% to Silver"; see describeAverageRank), or
        // "Unavailable". Never the bare number.
        Q_PROPERTY(QString averageRank READ averageRank NOTIFY changed)
        // The average rank as tier bar chunk fills (rankChunkFills), and the completed rank as a
        // position on that bar's chunk scale (the end of its chunk), or -1 when none is completed.
        Q_PROPERTY(QVariantList averageRankFills READ averageRankFills NOTIFY changed)
        Q_PROPERTY(double completedRankPosition READ completedRankPosition NOTIFY changed)
        Q_PROPERTY(QString totalPlaytime READ totalPlaytime NOTIFY changed)
        Q_PROPERTY(int scenariosAtNextTier READ scenariosAtNextTier NOTIFY changed)
        // Scenarios that keep the benchmark from nextTier: exactly the rows the progress grid marks as
        // blockers. hasNextTier is false when the top tier is attained (or the benchmark is not trackable).
        Q_PROPERTY(int nextTierBlockerCount READ nextTierBlockerCount NOTIFY changed)
        Q_PROPERTY(bool hasNextTier READ hasNextTier NOTIFY changed)
        Q_PROPERTY(int scenarioCount READ scenarioCount NOTIFY changed)
        Q_PROPERTY(ksv::presentation::BenchmarkHistoryViewModel *rankHistory READ rankHistory CONSTANT)
        Q_PROPERTY(ksv::presentation::BenchmarkHistoryViewModel *playtimeHistory READ playtimeHistory CONSTANT)
        Q_PROPERTY(ksv::presentation::BenchmarkProgressGridModel *progressGrid READ progressGrid CONSTANT)

    public:
        explicit BenchmarkTrackingViewModel(std::shared_ptr<application::IBenchmarkTrackingUseCase> useCase,
                                            QObject *parent = nullptr);

        [[nodiscard]] QString state() const { return m_state; }
        [[nodiscard]] QString stateMessage() const { return m_stateMessage; }
        [[nodiscard]] QString selectedName() const { return m_selectedName; }
        [[nodiscard]] QVariantList selectorEntries() const { return m_selectorEntries; }
        [[nodiscard]] QStringList completenessIssues() const { return m_completenessIssues; }
        [[nodiscard]] bool summaryAvailable() const { return m_summaryAvailable; }
        [[nodiscard]] QString attainedRank() const { return m_attainedRank; }
        [[nodiscard]] QString completedRank() const { return m_completedRank; }
        [[nodiscard]] QString nextTier() const { return m_nextTier; }
        [[nodiscard]] QString attainedRankColor() const { return m_attainedRankColor; }
        [[nodiscard]] QString completedRankColor() const { return m_completedRankColor; }
        [[nodiscard]] QString nextTierColor() const { return m_nextTierColor; }
        [[nodiscard]] QString averageRank() const { return m_averageRank; }
        [[nodiscard]] QVariantList averageRankFills() const { return m_averageRankFills; }
        [[nodiscard]] double completedRankPosition() const { return m_completedRankPosition; }
        [[nodiscard]] QString totalPlaytime() const { return m_totalPlaytime; }
        [[nodiscard]] int scenariosAtNextTier() const { return m_scenariosAtNextTier; }
        [[nodiscard]] int nextTierBlockerCount() const { return m_nextTierBlockerCount; }
        [[nodiscard]] bool hasNextTier() const { return m_hasNextTier; }
        [[nodiscard]] int scenarioCount() const { return m_scenarioCount; }
        [[nodiscard]] BenchmarkHistoryViewModel *rankHistory() const { return m_rankHistory; }
        [[nodiscard]] BenchmarkHistoryViewModel *playtimeHistory() const { return m_playtimeHistory; }
        [[nodiscard]] BenchmarkProgressGridModel *progressGrid() const { return m_progressGrid; }

        Q_INVOKABLE void selectBenchmark(const QString &id);
        Q_INVOKABLE void clearSelection();
        // Called when the workspace is shown with nothing selected; the use case decides whether
        // a remembered or sole benchmark should be shown.
        Q_INVOKABLE void selectDefaultBenchmark();

    signals:
        void changed();

    private:
        using ExtentMs = std::pair<qint64, qint64>;

        void rebuild();
        // A style-only change: each graph restyles itself; no measurement is read or recomputed.
        void applyPresentation();
        // A window or range change: only the playtime history and its axis move.
        void rebuildPlaytime();
        // A rank range change: only the rank history and its axis move.
        void rebuildRank();
        QList<QPointF> playtimePoints(const application::BenchmarkWorkspaceSnapshot &snapshot);
        [[nodiscard]] static BenchmarkHistoryViewModel::Content playtimeContent(
            const application::BenchmarkWorkspaceSnapshot &snapshot, const QList<QPointF> &points);
        void updateRankHistory(const application::BenchmarkWorkspaceSnapshot &snapshot);
        // The union both graphs default to; empty when neither has a sample.
        [[nodiscard]] std::optional<ExtentMs> unionExtent() const;
        // A graph with a date range spans exactly it; without one it spans the last applied union,
        // which either graph only reads and neither graph's range ever writes.
        void applyAxis(DateTimeAxis &axis, const std::optional<application::DayInterval> &interval) const;
        void applyRankAxis(const application::BenchmarkWorkspaceSnapshot &snapshot);
        void applyPlaytimeAxis(const application::BenchmarkWorkspaceSnapshot &snapshot);

        std::shared_ptr<application::IBenchmarkTrackingUseCase> m_useCase;
        // Must precede m_rankHistory/m_playtimeHistory: each child holds a non-owning pointer to its
        // axis and is constructed with a reference to it, so the axes must outlive them.
        DateTimeAxis m_historyXAxis;
        DateTimeAxis m_playtimeXAxis;
        BenchmarkHistoryViewModel *m_rankHistory;
        BenchmarkHistoryViewModel *m_playtimeHistory;
        BenchmarkProgressGridModel *m_progressGrid;

        // Of the full rank history, whatever its range shows.
        std::optional<ExtentMs> m_rankExtentMs;
        // Of the full-history playtime series, not of what its range shows.
        std::optional<ExtentMs> m_playtimeExtentMs;
        std::optional<ExtentMs> m_appliedUnionMs;
        // Kept from the last full rebuild, so a rank range change redraws without re-deriving them.
        bool m_rankPresent = false;
        std::vector<RankTier> m_rankTiers;

        QString m_state;
        QString m_stateMessage;
        QString m_selectedName;
        QVariantList m_selectorEntries;
        QStringList m_completenessIssues;
        bool m_summaryAvailable = false;
        QString m_attainedRank;
        QString m_completedRank;
        QString m_nextTier;
        QString m_attainedRankColor;
        QString m_completedRankColor;
        QString m_nextTierColor;
        QString m_averageRank;
        QVariantList m_averageRankFills;
        double m_completedRankPosition = -1.0;
        QString m_totalPlaytime;
        int m_scenariosAtNextTier = 0;
        int m_nextTierBlockerCount = 0;
        bool m_hasNextTier = false;
        int m_scenarioCount = 0;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H
