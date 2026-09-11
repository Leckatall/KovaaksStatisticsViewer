#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_TRACKING_USE_CASE_H

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include <QtGlobal>

#include "contracts/benchmark_workspace_snapshot.h"
#include "contracts/i_benchmark_tracking_use_case.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_projection.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"

namespace ksv::tests_ui {
    // The only dependency BenchmarkTrackingViewModel and its child models are allowed: a single
    // IBenchmarkTrackingUseCase. `snap` is the mutable revision; assign a new one and call
    // publish() to drive an onChanged notification exactly as the real use case would.
    class FakeBenchmarkTrackingUseCase final : public application::IBenchmarkTrackingUseCase {
    public:
        application::BenchmarkWorkspaceSnapshot snap;
        std::vector<domain::BenchmarkId> selectCalls;
        int clearCalls = 0;

        void select(const domain::BenchmarkId &id) override { selectCalls.push_back(id); }
        void clearSelection() override { ++clearCalls; }
        [[nodiscard]] const application::BenchmarkWorkspaceSnapshot &snapshot() const override { return snap; }
        [[nodiscard]] std::optional<domain::BenchmarkId> selected() const override { return snap.selectedId; }
        [[nodiscard]] application::BenchmarkTrackingState state() const override { return snap.availability; }
        [[nodiscard]] const domain::BenchmarkProjection *projection() const override {
            return snap.projection ? &*snap.projection : nullptr;
        }
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }

        void publish() const {
            for (const auto &callback: m_callbacks)
                if (callback) callback();
        }

    private:
        std::vector<std::function<void()> > m_callbacks;
    };

    inline std::chrono::sys_days ymd(const int year, const unsigned month, const unsigned day) {
        return std::chrono::sys_days{
            std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}};
    }

    inline qint64 utcMidnightMs(const std::chrono::sys_days day) {
        const auto epochDays = static_cast<long long>(day.time_since_epoch().count());
        return QDateTime(QDate(1970, 1, 1).addDays(epochDays), QTime(0, 0), QTimeZone::utc())
            .toMSecsSinceEpoch();
    }

    inline domain::Tier vtTier(const std::string &id, const std::string &name) {
        return {domain::TierId{id}, name, {}};
    }

    inline application::BenchmarkChoice vtChoice(std::string filename, std::optional<std::string> id,
                                                std::string displayName,
                                                application::BenchmarkChoiceClassification classification,
                                                const bool selectable) {
        application::BenchmarkChoice choice;
        choice.filename = std::move(filename);
        if (id) choice.id = domain::BenchmarkId{*id};
        choice.displayName = std::move(displayName);
        choice.classification = classification;
        choice.selectable = selectable;
        return choice;
    }

    inline domain::ScenarioProjection vtScenario(const std::string &id, const std::string &name,
                                                 const std::optional<double> personalBest,
                                                 const std::optional<double> recentAverage,
                                                 const int recentSampleCount,
                                                 const std::optional<std::string> &attainedTier,
                                                 const std::optional<domain::Threshold> &nextThreshold,
                                                 const domain::ScenarioMatchState matchState) {
        domain::ScenarioProjection scenario;
        scenario.entryId = domain::ScenarioEntryId{id};
        scenario.name = name;
        scenario.matchState = matchState;
        scenario.personalBest = personalBest;
        scenario.recentAverage = recentAverage;
        scenario.recentSampleCount = recentSampleCount;
        scenario.runCount = recentSampleCount;
        if (attainedTier) scenario.attainedTier = domain::TierId{*attainedTier};
        scenario.nextThreshold = nextThreshold;
        return scenario;
    }

    // A Ready + Trackable revision for benchmark "b1": three tiers, one "Clicking" category holding
    // "s1", one uncategorized "s2", attained Silver with Gold next, two blockers, and two-day rank
    // and rolling-playtime histories.
    inline application::BenchmarkWorkspaceSnapshot makeTrackableSnapshot() {
        domain::Benchmark definition;
        definition.id = domain::BenchmarkId{"b1"};
        definition.name = "Voltaic";
        definition.tiers = {vtTier("t1", "Bronze"), vtTier("t2", "Silver"), vtTier("t3", "Gold")};

        domain::ScenarioEntry s1{domain::ScenarioEntryId{"s1"}, "1w6ts", std::string{"h1"}, {}};
        domain::ScenarioEntry s2{domain::ScenarioEntryId{"s2"}, "pasu", std::nullopt, {}};
        domain::Category clicking{domain::GroupId{"c1"}, "Clicking", {}, {s1}, {}};
        definition.categories = {clicking};
        definition.uncategorized = {s2};

        domain::BenchmarkProjection projection;
        projection.benchmarkId = domain::BenchmarkId{"b1"};
        projection.completeness = domain::Completeness::Trackable;
        projection.tiers = definition.tiers;
        projection.attainedRank = domain::TierId{"t2"};
        projection.completedRank = std::nullopt;
        projection.nextTier = domain::TierId{"t3"};
        projection.averageRank = 2.5;
        projection.totalPlaytimeSeconds = 3661.0;
        projection.scenariosAtNextTier = 1;
        projection.nextTierBlockers = {
            domain::RankBlocker{domain::ScenarioEntryId{"s1"}, domain::GroupId{"c1"}, 850.0, 800.0},
            domain::RankBlocker{domain::ScenarioEntryId{"s2"}, std::nullopt, 900.0, std::nullopt},
        };
        projection.scenarios = {
            vtScenario("s1", "1w6ts", 800.0, 790.0, 3, std::string{"t1"},
                       domain::Threshold{domain::TierId{"t2"}, 850.0}, domain::ScenarioMatchState::Resolved),
            vtScenario("s2", "pasu", std::nullopt, std::nullopt, 0, std::nullopt, std::nullopt,
                       domain::ScenarioMatchState::Unresolved),
        };
        projection.uncategorized = {domain::ScenarioEntryId{"s2"}};
        projection.categories = {
            domain::GroupProjection{domain::GroupId{"c1"}, "Clicking", domain::TierId{"t1"},
                                    {domain::ScenarioEntryId{"s1"}}, {}},
        };
        projection.averageRankHistory = {
            {ymd(2024, 3, 4), 1.0},
            {ymd(2024, 3, 6), 2.5},
        };
        projection.rollingPlaytime = {
            {ymd(2024, 3, 4), 1800.0},
            {ymd(2024, 3, 6), 900.0},
        };

        application::BenchmarkWorkspaceSnapshot snapshot;
        snapshot.libraryRevision = 1;
        snapshot.choices = {vtChoice("b1.json", std::string{"b1"}, "Voltaic",
                                     application::BenchmarkChoiceClassification::Trackable, true)};
        snapshot.selectedId = domain::BenchmarkId{"b1"};
        snapshot.lastKnownSelectedDisplayName = "Voltaic";
        snapshot.availability = application::BenchmarkTrackingState::Ready;
        snapshot.selectedLoaded = data::LoadedBenchmark{
            definition, domain::CompletenessResult{domain::Completeness::Trackable, {}}};
        snapshot.projection = projection;
        return snapshot;
    }

    // A Ready + Trackable revision with a deeper hierarchy for the breakdown model: an
    // Uncategorized bucket ("u1" Unresolved, "u2" AutoMappable), a "Clicking" category with direct
    // scenarios "e1"/"e2", and a "Tracking" category whose only child is the "Smoothness"
    // subcategory holding "e3". Blockers name "e2" (under Clicking) and "e3" (under Smoothness).
    inline application::BenchmarkWorkspaceSnapshot makeBreakdownSnapshot() {
        domain::Benchmark definition;
        definition.id = domain::BenchmarkId{"b1"};
        definition.name = "Voltaic";
        definition.tiers = {vtTier("t1", "Bronze"), vtTier("t2", "Silver"), vtTier("t3", "Gold")};

        domain::ScenarioEntry u1{domain::ScenarioEntryId{"u1"}, "PlainOne", std::nullopt, {}};
        domain::ScenarioEntry u2{domain::ScenarioEntryId{"u2"}, "PlainTwo", std::nullopt, {}};
        domain::ScenarioEntry e1{domain::ScenarioEntryId{"e1"}, "1w6ts", std::string{"h1"}, {}};
        domain::ScenarioEntry e2{domain::ScenarioEntryId{"e2"}, "pasu", std::string{"gone"}, {}};
        domain::ScenarioEntry e3{domain::ScenarioEntryId{"e3"}, "smoothbot", std::string{"h3"}, {}};
        definition.uncategorized = {u1, u2};

        domain::Category clicking{domain::GroupId{"c1"}, "Clicking",
                                  domain::BenchmarkColor{255, 0, 0}, {e1, e2}, {}};
        domain::Subcategory smoothness{domain::GroupId{"sc1"}, "Smoothness",
                                       domain::BenchmarkColor{0, 200, 0}, {e3}};
        domain::Category tracking{domain::GroupId{"c2"}, "Tracking",
                                  domain::BenchmarkColor{0, 128, 255}, {}, {smoothness}};
        definition.categories = {clicking, tracking};

        domain::BenchmarkProjection projection;
        projection.benchmarkId = domain::BenchmarkId{"b1"};
        projection.completeness = domain::Completeness::Trackable;
        projection.tiers = definition.tiers;
        projection.attainedRank = domain::TierId{"t1"};
        projection.nextTier = domain::TierId{"t2"};
        projection.averageRank = 1.5;
        projection.totalPlaytimeSeconds = 7200.0;
        projection.uncategorized = {domain::ScenarioEntryId{"u1"}, domain::ScenarioEntryId{"u2"}};
        projection.categories = {
            domain::GroupProjection{domain::GroupId{"c1"}, "Clicking", domain::TierId{"t1"},
                                    {domain::ScenarioEntryId{"e1"}, domain::ScenarioEntryId{"e2"}}, {}},
            domain::GroupProjection{
                domain::GroupId{"c2"}, "Tracking", std::nullopt, {},
                {domain::GroupProjection{domain::GroupId{"sc1"}, "Smoothness", domain::TierId{"t2"},
                                         {domain::ScenarioEntryId{"e3"}}, {}}}},
        };
        projection.scenarios = {
            vtScenario("u1", "PlainOne", 100.0, 90.0, 2, std::string{"t1"},
                       domain::Threshold{domain::TierId{"t2"}, 150.0}, domain::ScenarioMatchState::Unresolved),
            vtScenario("u2", "PlainTwo", std::nullopt, std::nullopt, 0, std::nullopt, std::nullopt,
                       domain::ScenarioMatchState::AutoMappable),
            vtScenario("e1", "1w6ts", 800.0, 780.0, 3, std::string{"t1"},
                       domain::Threshold{domain::TierId{"t2"}, 850.0}, domain::ScenarioMatchState::Resolved),
            vtScenario("e2", "pasu", std::nullopt, std::nullopt, 0, std::nullopt, std::nullopt,
                       domain::ScenarioMatchState::MappedUnavailable),
            vtScenario("e3", "smoothbot", 500.0, 490.0, 5, std::nullopt,
                       domain::Threshold{domain::TierId{"t1"}, 300.0}, domain::ScenarioMatchState::Ambiguous),
        };
        projection.nextTierBlockers = {
            domain::RankBlocker{domain::ScenarioEntryId{"e2"}, domain::GroupId{"c1"}, 850.0, std::nullopt},
            domain::RankBlocker{domain::ScenarioEntryId{"e3"}, domain::GroupId{"sc1"}, 300.0, 500.0},
        };
        projection.scenariosAtNextTier = 2;

        application::BenchmarkWorkspaceSnapshot snapshot;
        snapshot.libraryRevision = 1;
        snapshot.choices = {vtChoice("b1.json", std::string{"b1"}, "Voltaic",
                                     application::BenchmarkChoiceClassification::Trackable, true)};
        snapshot.selectedId = domain::BenchmarkId{"b1"};
        snapshot.lastKnownSelectedDisplayName = "Voltaic";
        snapshot.availability = application::BenchmarkTrackingState::Ready;
        snapshot.selectedLoaded = data::LoadedBenchmark{
            definition, domain::CompletenessResult{domain::Completeness::Trackable, {}}};
        snapshot.projection = projection;
        return snapshot;
    }
}

#endif
