#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_TRACKING_USE_CASE_H

#include <chrono>
#include <functional>
#include <initializer_list>
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
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"
#include "benchmark_builders.h"

namespace ksv::tests_ui {
    struct BenchmarkFixture {
        domain::BenchmarkDefinition definition;
        std::vector<domain::RunFact> runs;
        std::vector<domain::ScenarioResolution> resolutions;
        std::optional<domain::CompletenessResult> completeness;

        [[nodiscard]] domain::Benchmark evaluate() const {
            if (!completeness) return tests_support::evaluatedBenchmark(definition, runs, resolutions);

            auto matches = resolutions;
            if (matches.empty()) {
                for (const auto *entry: definition.entries())
                    matches.push_back(tests_support::resolvedEntry(entry->id.value));
            }
            return domain::Benchmark(definition, *completeness, matches, runs);
        }
    };

    inline application::BenchmarkWorkspaceSnapshot snapshotFor(const BenchmarkFixture &fixture);

    // The only dependency BenchmarkTrackingViewModel and its child models are allowed: a single
    // IBenchmarkTrackingUseCase. `snap` is the mutable revision; assign a new one and call
    // publish() to drive an onChanged notification exactly as the real use case would.
    class FakeBenchmarkTrackingUseCase final : public application::IBenchmarkTrackingUseCase {
    public:
        application::BenchmarkWorkspaceSnapshot snap;
        BenchmarkFixture fixture;

        void show(BenchmarkFixture next) {
            fixture = std::move(next);
            snap = snapshotFor(fixture);
        }

        void reevaluate() { snap.benchmark = fixture.evaluate(); }

        std::vector<domain::BenchmarkId> selectCalls;
        int clearCalls = 0;
        int selectDefaultCalls = 0;

        void select(const domain::BenchmarkId &id) override { selectCalls.push_back(id); }
        void clearSelection() override { ++clearCalls; }
        void selectDefault() override { ++selectDefaultCalls; }
        mutable int snapshotCalls = 0;
        [[nodiscard]] const application::BenchmarkWorkspaceSnapshot &snapshot() const override {
            ++snapshotCalls;
            return snap;
        }
        [[nodiscard]] std::optional<domain::BenchmarkId> selected() const override { return snap.selectedId; }
        [[nodiscard]] application::BenchmarkTrackingState state() const override { return snap.availability; }
        [[nodiscard]] const domain::Benchmark *benchmark() const override {
            return snap.availability == application::BenchmarkTrackingState::Ready && snap.benchmark
                       ? &*snap.benchmark : nullptr;
        }
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }
        void onPlaytimeChanged(std::function<void()> callback) override {
            m_playtimeCallbacks.push_back(std::move(callback));
        }
        void onRankChanged(std::function<void(application::GraphKey)> callback) override {
            m_rankCallbacks.push_back(std::move(callback));
        }
        // The calendar axis and the rank axis that starts at Unranked, so tests written before either
        // was a choice read as they always have; run-axis and fitted-axis tests opt in.
        application::GraphPresentation rankPresentation =
            asOriginally(application::defaultGraphPresentation(application::GraphKey::BenchmarkRank));
        application::GraphPresentation recentRankPresentation =
            asOriginally(application::defaultGraphPresentation(application::GraphKey::BenchmarkRecentRank));
        application::GraphPresentation playtimePresentation =
            application::defaultGraphPresentation(application::GraphKey::BenchmarkPlaytime);
        [[nodiscard]] application::GraphPresentation presentation(const application::GraphKey graph) const override {
            switch (graph) {
                case application::GraphKey::BenchmarkRank: return rankPresentation;
                case application::GraphKey::BenchmarkRecentRank: return recentRankPresentation;
                default: return playtimePresentation;
            }
        }
        void onPresentationChanged(std::function<void()> callback) override {
            m_presentationCallbacks.push_back(std::move(callback));
        }

        void publishPresentation() const {
            for (const auto &callback: m_presentationCallbacks)
                if (callback) callback();
        }

        void publish() const {
            for (const auto &callback: m_callbacks)
                if (callback) callback();
        }

        void publishPlaytime() const {
            for (const auto &callback: m_playtimeCallbacks)
                if (callback) callback();
        }

        void publishRank(const application::GraphKey graph = application::GraphKey::BenchmarkRank) const {
            for (const auto &callback: m_rankCallbacks)
                if (callback) callback(graph);
        }

    private:
        static application::GraphPresentation asOriginally(application::GraphPresentation presentation) {
            presentation.xAxis = std::string(application::kCalendarXAxis);
            presentation.axisPolicies["rank"] = application::StartAtZero{};
            return presentation;
        }

        std::vector<std::function<void()> > m_callbacks;
        std::vector<std::function<void()> > m_playtimeCallbacks;
        std::vector<std::function<void(application::GraphKey)> > m_rankCallbacks;
        std::vector<std::function<void()> > m_presentationCallbacks;
    };

    inline std::chrono::sys_days ymd(const int year, const unsigned month, const unsigned day) {
        return std::chrono::sys_days{
            std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}};
    }

    inline qint64 expectedMidnightMs(const std::chrono::sys_days day) {
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

    inline std::vector<domain::Tier> standardTiers() {
        return {vtTier("t1", "Bronze"), vtTier("t2", "Silver"), vtTier("t3", "Gold")};
    }

    inline std::vector<domain::Threshold> standardThresholds(double bronze, double silver, double gold) {
        return {{{"t1"}, bronze}, {{"t2"}, silver}, {{"t3"}, gold}};
    }

    inline domain::RunFact runOn(std::chrono::sys_days day, const std::string &hash,
                                 float score, float duration = 0.0F) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(day.time_since_epoch()).count();
        return tests_support::benchmarkFact(hash, ms, score, duration);
    }

    inline BenchmarkFixture makeTrackableFixture() {
        using namespace tests_support;
        BenchmarkFixture fixture;
        auto &definition = fixture.definition;
        definition.id = domain::BenchmarkId{"b1"};
        definition.name = "Voltaic";
        definition.tiers = standardTiers();
        definition.uncategorized = {
            benchmarkEntry("s2", "pasu", "h2", {{"t1", 500}, {"t2", 700}, {"t3", 900}})};
        definition.categories = {
            {domain::GroupId{"c1"}, "Clicking", {},
             {benchmarkEntry("s1", "1w6ts", "h1", {{"t1", 400}, {"t2", 600}, {"t3", 1000}})}, {}}};
        fixture.runs = {runOn(ymd(2024, 3, 4), "h1", 600, 1800),
                        runOn(ymd(2024, 3, 5), "h1", 500),
                        runOn(ymd(2024, 3, 6), "h1", 800, 900),
                        runOn(ymd(2024, 3, 6), "h2", 800, 961)};
        return fixture;
    }

    inline BenchmarkFixture makeBreakdownFixture() {
        using namespace tests_support;
        BenchmarkFixture fixture;
        auto &definition = fixture.definition;
        definition.id = domain::BenchmarkId{"b1"};
        definition.name = "Voltaic";
        definition.tiers = standardTiers();
        const std::vector<std::pair<std::string, double>> defaultThresholds{
            {"t1", 600}, {"t2", 900}, {"t3", 1200}};
        definition.uncategorized = {benchmarkEntry("u1", "PlainOne", std::nullopt, defaultThresholds),
                                    benchmarkEntry("u2", "PlainTwo", std::nullopt, defaultThresholds)};
        definition.categories = {
            {domain::GroupId{"c1"}, "Clicking", {255, 0, 0},
             {benchmarkEntry("e1", "1w6ts", "h1", {{"t1", 500}, {"t2", 850}, {"t3", 1200}}),
              benchmarkEntry("e2", "pasu", "gone", defaultThresholds)}, {}},
            {domain::GroupId{"c2"}, "Tracking", {0, 128, 255}, {},
             {{domain::GroupId{"sc1"}, "Smoothness", {0, 200, 0},
               {benchmarkEntry("e3", "smoothbot", "h3", {{"t1", 300}, {"t2", 600}, {"t3", 900}})}}}}};
        fixture.resolutions = {unresolvedEntry("u1"), autoMappableEntry("u2", "candidate"),
                               resolvedEntry("e1"), mappedUnavailableEntry("e2"),
                               ambiguousEntry("e3", {"h3", "candidate"})};
        fixture.runs = {runOn(ymd(2024, 3, 4), "h1", 760, 60), runOn(ymd(2024, 3, 5), "h1", 800, 60)};
        return fixture;
    }

    inline BenchmarkFixture makeUniformFixture(
        int tierCount, std::initializer_list<std::pair<int, float>> dayScores, double thresholdStep = 1.0) {
        BenchmarkFixture fixture;
        auto &definition = fixture.definition;
        definition.id = domain::BenchmarkId{"b1"};
        definition.name = "Voltaic";
        auto entry = tests_support::benchmarkEntry("u", "Uniform", "u", {});
        if (tierCount == 3) definition.tiers = standardTiers();
        for (int rank = 1; rank <= tierCount; ++rank) {
            const auto id = "t" + std::to_string(rank);
            if (tierCount != 3) definition.tiers.push_back(vtTier(id, "Tier " + std::to_string(rank)));
            entry.thresholds.push_back({domain::TierId{id}, rank * thresholdStep});
        }
        definition.uncategorized.push_back(std::move(entry));
        for (const auto &[day, score]: dayScores)
            fixture.runs.push_back(runOn(ymd(2024, 3, 4) + std::chrono::days(day - 4), "u", score, 60));
        return fixture;
    }

    inline application::BenchmarkWorkspaceSnapshot snapshotFor(const BenchmarkFixture &fixture) {
        application::BenchmarkWorkspaceSnapshot snapshot;
        snapshot.libraryRevision = 1;
        snapshot.selectedId = fixture.definition.id;
        snapshot.lastKnownSelectedDisplayName = fixture.definition.name;
        snapshot.availability = application::BenchmarkTrackingState::Ready;
        snapshot.benchmark = fixture.evaluate();
        const auto classification = snapshot.benchmark->completeness().completeness == domain::Completeness::Trackable
                                        ? application::BenchmarkChoiceClassification::Trackable
                                        : application::BenchmarkChoiceClassification::Incomplete;
        snapshot.choices = {vtChoice(fixture.definition.id.value + ".json", fixture.definition.id.value,
                                     fixture.definition.name, classification, true)};
        snapshot.visiblePlaytime = {{ymd(2024, 3, 4), 1800.0}, {ymd(2024, 3, 6), 900.0}};
        return snapshot;
    }

    inline application::BenchmarkWorkspaceSnapshot makeTrackableSnapshot() {
        return snapshotFor(makeTrackableFixture());
    }

    inline application::BenchmarkWorkspaceSnapshot makeBreakdownSnapshot() {
        return snapshotFor(makeBreakdownFixture());
    }
}

#endif
