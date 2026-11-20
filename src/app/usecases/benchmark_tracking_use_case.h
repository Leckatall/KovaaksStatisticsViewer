#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "contracts/benchmark_workspace_snapshot.h"
#include "contracts/i_benchmark_resolution_use_case.h"
#include "contracts/i_benchmark_tracking_use_case.h"
#include "contracts/i_graph_configuration_use_case.h"
#include "contracts/i_recent_benchmarks.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "data/interfaces/i_day_source.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class BenchmarkTrackingUseCase final : public IBenchmarkTrackingUseCase {
    public:
        // `recent` is the list of viewed benchmarks that survives launches; without it (tests)
        // nothing is remembered and every choice's recency is empty.
        BenchmarkTrackingUseCase(std::shared_ptr<data::IBenchmarksService> benchmarks,
                                 std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
                                 std::shared_ptr<IProfileService> profileService,
                                 std::shared_ptr<IGraphConfigurationUseCase> configuration,
                                 std::shared_ptr<data::IDaySource> daySource,
                                 std::shared_ptr<IRecentBenchmarks> recent = nullptr);

        void select(const domain::BenchmarkId &id) override;
        void clearSelection() override;
        void selectDefault() override;
        [[nodiscard]] const BenchmarkWorkspaceSnapshot &snapshot() const override { return m_snapshot; }
        [[nodiscard]] std::optional<domain::BenchmarkId> selected() const override { return m_selected; }
        [[nodiscard]] BenchmarkTrackingState state() const override { return m_state; }
        // TODO(2026-11-19): Remove at Cleanup with BenchmarkProjection; replaced by benchmark()
        [[nodiscard]] const domain::BenchmarkProjection *projection() const override {
            return m_state == BenchmarkTrackingState::Ready ? &m_projection : nullptr;
        }
        [[nodiscard]] const domain::Benchmark *benchmark() const override {
            return m_state == BenchmarkTrackingState::Ready ? &*m_snapshot.benchmark : nullptr;
        }
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }
        void onPlaytimeChanged(std::function<void()> callback) override {
            m_playtimeCallbacks.push_back(std::move(callback));
        }
        void onRankChanged(std::function<void(GraphKey)> callback) override {
            m_rankCallbacks.push_back(std::move(callback));
        }
        [[nodiscard]] GraphPresentation presentation(const GraphKey graph) const override {
            return presentationOf(graph, m_configuration->current().configuration->preferences(graph));
        }
        void onPresentationChanged(std::function<void()> callback) override {
            m_presentationCallbacks.push_back(std::move(callback));
        }

    private:
        // Everything derived from one benchmark's definition and the profile's runs. The playtime
        // series is kept apart from the benchmark so a window or range change needs no re-evaluation.
        struct Evaluation {
            domain::Benchmark benchmark;
            std::optional<int> fullPlaytimeWindowDays;
            PlaytimeSeries fullPlaytime;
        };

        void refresh();
        void notifyChanged() const;
        void notifyPlaytimeChanged() const;
        [[nodiscard]] Evaluation evaluate(const data::LoadedBenchmark &loaded) const;
        void applyPlaytimeView(Evaluation &evaluation);
        void refreshPlaytimeOnly();
        // Resolves a rank graph's range into its snapshot view: BenchmarkRank into `rank`,
        // BenchmarkRecentRank into `recentRank`. Neither rank history is ever filtered, so a range
        // change needs no re-evaluation.
        void applyRankRange(GraphKey graph, const Evaluation &evaluation);
        void refreshRankOnly(GraphKey graph);

        std::shared_ptr<data::IBenchmarksService> m_benchmarks;
        std::shared_ptr<IBenchmarkResolutionUseCase> m_resolution;
        std::shared_ptr<IProfileService> m_profileService;
        std::shared_ptr<IGraphConfigurationUseCase> m_configuration;
        std::shared_ptr<data::IDaySource> m_daySource;
        std::shared_ptr<IRecentBenchmarks> m_recent;
        // Set while select() records its own markViewed, whose onChanged must not announce a second time.
        bool m_markingViewed = false;
        // Cached whole-library snapshot; the service returns it by value (a deep copy of every
        // definition), so it is re-fetched only when the library revision moves, not on every
        // profile change that merely invalidates evaluations.
        std::optional<data::BenchmarkLibrarySnapshot> m_librarySnapshot;
        std::uint64_t m_librarySnapshotRevision = 0;
        bool m_librarySnapshotValid = false;
        std::optional<domain::BenchmarkId> m_selected;
        // Last display name m_selected resolved to from a choice; carried into the snapshot on
        // revisions where the selection has vanished or degraded to a problem with no parsed id,
        // so Unavailable can still name what the user picked.
        std::string m_lastKnownSelectedDisplayName;
        BenchmarkTrackingState m_state = BenchmarkTrackingState::NoSelection;
        // TODO(2026-11-19): Remove at Cleanup with BenchmarkProjection; replaced by benchmark()
        domain::BenchmarkProjection m_projection;
        std::map<domain::BenchmarkId, Evaluation> m_cache;
        BenchmarkWorkspaceSnapshot m_snapshot;
        std::vector<std::function<void()> > m_callbacks;
        std::vector<std::function<void()> > m_playtimeCallbacks;
        std::vector<std::function<void(GraphKey)> > m_rankCallbacks;
        std::vector<std::function<void()> > m_presentationCallbacks;
    };
}

#endif
