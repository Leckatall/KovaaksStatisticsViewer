#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <qqmlintegration.h>

#include <memory>

#include "app/contracts/benchmark_workspace_snapshot.h"
#include "app/contracts/i_benchmark_tracking_use_case.h"
#include "benchmark_breakdown_model.h"
#include "benchmark_history_vm.h"
#include "date_time_axis.h"

namespace ksv::presentation {
    // Adapts one IBenchmarkTrackingUseCase revision into the benchmark workspace's QML surface:
    // selector entries, explicit state, status summary, blockers, and three owned presentation-only
    // child models (two history graphs and the hierarchical breakdown). Its only dependency is the
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
        Q_PROPERTY(QString averageRank READ averageRank NOTIFY changed)
        Q_PROPERTY(QString totalPlaytime READ totalPlaytime NOTIFY changed)
        Q_PROPERTY(int scenariosAtNextTier READ scenariosAtNextTier NOTIFY changed)
        Q_PROPERTY(int scenarioCount READ scenarioCount NOTIFY changed)
        Q_PROPERTY(QVariantList blockers READ blockers NOTIFY changed)
        Q_PROPERTY(ksv::presentation::BenchmarkHistoryViewModel *rankHistory READ rankHistory CONSTANT)
        Q_PROPERTY(ksv::presentation::BenchmarkHistoryViewModel *playtimeHistory READ playtimeHistory CONSTANT)
        Q_PROPERTY(ksv::presentation::BenchmarkBreakdownModel *breakdown READ breakdown CONSTANT)

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
        [[nodiscard]] QString averageRank() const { return m_averageRank; }
        [[nodiscard]] QString totalPlaytime() const { return m_totalPlaytime; }
        [[nodiscard]] int scenariosAtNextTier() const { return m_scenariosAtNextTier; }
        [[nodiscard]] int scenarioCount() const { return m_scenarioCount; }
        [[nodiscard]] QVariantList blockers() const { return m_blockers; }
        [[nodiscard]] BenchmarkHistoryViewModel *rankHistory() const { return m_rankHistory; }
        [[nodiscard]] BenchmarkHistoryViewModel *playtimeHistory() const { return m_playtimeHistory; }
        [[nodiscard]] BenchmarkBreakdownModel *breakdown() const { return m_breakdown; }

        Q_INVOKABLE void selectBenchmark(const QString &id);
        Q_INVOKABLE void clearSelection();
        Q_INVOKABLE void setExpanded(const QString &nodeId, bool expanded);

    signals:
        void changed();

    private:
        void rebuild();

        std::shared_ptr<application::IBenchmarkTrackingUseCase> m_useCase;
        // Must precede m_rankHistory/m_playtimeHistory: both children hold a non-owning pointer to
        // this axis and are constructed with a reference to it, so it must outlive them.
        DateTimeAxis m_historyXAxis;
        BenchmarkHistoryViewModel *m_rankHistory;
        BenchmarkHistoryViewModel *m_playtimeHistory;
        BenchmarkBreakdownModel *m_breakdown;

        QString m_state;
        QString m_stateMessage;
        QString m_selectedName;
        QVariantList m_selectorEntries;
        QStringList m_completenessIssues;
        bool m_summaryAvailable = false;
        QString m_attainedRank;
        QString m_completedRank;
        QString m_nextTier;
        QString m_averageRank;
        QString m_totalPlaytime;
        int m_scenariosAtNextTier = 0;
        int m_scenarioCount = 0;
        QVariantList m_blockers;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_VM_H
