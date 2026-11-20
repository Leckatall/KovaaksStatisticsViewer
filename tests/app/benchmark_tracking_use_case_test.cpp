#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "benchmark_builders.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_validation.h"
#include "fake_benchmark_store.h"
#include "fake_day_source.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"
#include "data/benchmarks_service.h"
#include "usecases/benchmark_resolution_use_case.h"
#include "usecases/benchmark_tracking_use_case.h"
#include "usecases/recent_benchmarks.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::data;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    constexpr long long kDay = 86'400'000LL;

    std::vector<std::pair<std::string, double> > rungs() {
        return {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
    }

    BenchmarkDefinition benchmark(const std::string &id) {
        BenchmarkDefinition value;
        value.id = BenchmarkId{id};
        value.name = "Tracked";
        value.tiers = {benchmarkTier("bronze"), benchmarkTier("silver"), benchmarkTier("gold")};
        value.uncategorized = {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs()),
                               benchmarkEntry("e2", "Beta", std::string{"h2"}, rungs())};
        return value;
    }

    BenchmarkFileEntry fileFor(const BenchmarkDefinition &value, const std::string &filename) {
        return {filename, "d-" + filename, LoadedBenchmark{value, validateBenchmark(value)}};
    }

    struct Fixture {
        std::shared_ptr<FakeBenchmarkStore> repo = std::make_shared<FakeBenchmarkStore>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        std::shared_ptr<BenchmarksService> benchmarks;
        std::shared_ptr<BenchmarkResolutionUseCase> resolution;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        GraphPreferences &playtimePreferences() {
            return configuration->configuration.preferences(GraphKey::BenchmarkPlaytime);
        }

        void commitPlaytime(const GraphKey key = GraphKey::BenchmarkPlaytime) {
            ++configuration->revision;
            configuration->publish({key});
        }

        void known(const std::string &name, const std::string &hash) {
            profile->scenarios.push_back({name, hash});
        }

        void build() {
            benchmarks = std::make_shared<BenchmarksService>(repo);
            resolution = std::make_shared<BenchmarkResolutionUseCase>(benchmarks, profile);
            tracking = std::make_unique<BenchmarkTrackingUseCase>(benchmarks, resolution, profile, configuration, days);
        }
    };

    BenchmarkLibrarySnapshot snapshotFor(const BenchmarkDefinition &value) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back(fileFor(value, value.id.value + ".json"));
        return snapshot;
    }

    std::vector<RunFact> useRealAccounting(Fixture &fixture, const std::vector<std::pair<long long, float>> &dailyRuns) {
        for (const auto &[day, seconds]: dailyRuns)
            fixture.profile->run_facts.push_back(benchmarkFact("h1", day * kDay, 250.0F, seconds));
        return fixture.profile->run_facts;
    }

    const std::vector<std::pair<long long, float>> kUnequalDailyRuns{
        {100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 45.0F}};

    void expectDistinctWindows(const Benchmark &reference, int window) {
        const int defaultWindow = effectiveAveragingWindow(defaultGraphPreferences(GraphKey::BenchmarkPlaytime));
        const auto series = reference.rollingPlaytime(window);
        ASSERT_FALSE(series.empty());
        ASSERT_NE(series, reference.rollingPlaytime(defaultWindow));
        ASSERT_NE(series, reference.rollingPlaytime(3));
    }

    void expectSameBenchmark(const Benchmark &actual, const Benchmark &expected, int window) {
        EXPECT_EQ(actual.attainedRank(), expected.attainedRank());
        EXPECT_EQ(actual.completedRank(), expected.completedRank());
        EXPECT_EQ(actual.nextTier(), expected.nextTier());
        EXPECT_EQ(actual.averageRank(), expected.averageRank());
        const auto compareHistory = [](const auto &actualHistory, const auto &expectedHistory) {
            ASSERT_EQ(actualHistory.size(), expectedHistory.size());
            for (std::size_t index = 0; index < actualHistory.size(); ++index) {
                EXPECT_EQ(actualHistory[index].day, expectedHistory[index].day);
                EXPECT_EQ(actualHistory[index].averageRank, expectedHistory[index].averageRank);
            }
        };
        compareHistory(actual.averageRankHistory(), expected.averageRankHistory());
        compareHistory(actual.recentAverageRankHistory(), expected.recentAverageRankHistory());
        EXPECT_DOUBLE_EQ(actual.totalPlaytimeSeconds(), expected.totalPlaytimeSeconds());
        EXPECT_EQ(actual.latestRunDay(), expected.latestRunDay());
        EXPECT_EQ(actual.rollingPlaytime(window), expected.rollingPlaytime(window));
        const auto actualScenarios = actual.scenarios();
        const auto expectedScenarios = expected.scenarios();
        ASSERT_EQ(actualScenarios.size(), expectedScenarios.size());
        for (std::size_t index = 0; index < actualScenarios.size(); ++index) {
            EXPECT_EQ(actualScenarios[index]->entryId(), expectedScenarios[index]->entryId());
            EXPECT_EQ(actualScenarios[index]->stats(), expectedScenarios[index]->stats());
            EXPECT_EQ(actualScenarios[index]->attainedTier(), expectedScenarios[index]->attainedTier());
            EXPECT_EQ(actualScenarios[index]->matchState(), expectedScenarios[index]->matchState());
        }
    }
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ReadySnapshotCarriesTheEvaluatedBenchmarkEqualToAnIndependentOne) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    const auto definition = benchmark("b1");
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    const auto expected = evaluatedBenchmark(definition, fixture.profile->run_facts);
    fixture.build();
    fixture.tracking->select(definition.id);

    const auto &snapshot = fixture.tracking->snapshot();
    ASSERT_EQ(snapshot.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snapshot.benchmark);
    EXPECT_EQ(fixture.tracking->benchmark(), &*snapshot.benchmark);
    expectSameBenchmark(*snapshot.benchmark, expected, snapshot.playtime.windowDays);
}

TEST(BenchmarkTrackingWorkspaceSnapshot, NoSelectionAndUnavailableClearTheBenchmark) {
    Fixture fixture;
    const auto definition = benchmark("b1");
    fixture.known("Alpha", "h1");
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.build();

    const auto expectCleared = [&](BenchmarkTrackingState state) {
        const auto &snapshot = fixture.tracking->snapshot();
        EXPECT_EQ(snapshot.availability, state);
        EXPECT_FALSE(snapshot.benchmark);
        EXPECT_TRUE(snapshot.visiblePlaytime.empty());
        EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
    };
    fixture.tracking->select(definition.id);
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    ASSERT_FALSE(fixture.tracking->snapshot().visiblePlaytime.empty());
    fixture.tracking->clearSelection();
    expectCleared(BenchmarkTrackingState::NoSelection);
    fixture.tracking->select(definition.id);
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    fixture.tracking->select(BenchmarkId{"missing"});
    expectCleared(BenchmarkTrackingState::Unavailable);
}

TEST(BenchmarkTrackingWorkspaceSnapshot, IncompleteBenchmarkCarriesCompletenessAndPerScenarioStatistics) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    auto definition = benchmark("b1");
    definition.uncategorized[1].thresholds.clear();
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    const auto expected = evaluatedBenchmark(definition, fixture.profile->run_facts,
                                             {resolvedEntry("e1"), mappedUnavailableEntry("e2")});
    fixture.build();
    fixture.tracking->select(definition.id);

    const auto *actual = fixture.tracking->benchmark();
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->completeness().completeness, Completeness::Incomplete);
    EXPECT_FALSE(actual->attainedRank());
    expectSameBenchmark(*actual, expected, fixture.tracking->snapshot().playtime.windowDays);
}

TEST(BenchmarkTracking, RequestsTheMappedHashesInFirstOccurrenceOrderAndNothingElse_R24) {
    Fixture fixture;
    auto definition = benchmark("b1");
    definition.uncategorized[0].hashes = {"h1", "new", "h2"};
    definition.uncategorized.push_back(benchmarkEntry("e3", "Again", std::string{"h1"}, rungs()));
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.build();
    fixture.tracking->select(definition.id);

    std::vector<std::string> hashes;
    for (const auto &scenario: fixture.profile->requested_fact_scenarios) {
        EXPECT_TRUE(scenario.name.empty());
        hashes.push_back(scenario.hash);
    }
    EXPECT_EQ(hashes, (std::vector<std::string>{"h1", "new", "h2"}));
    EXPECT_TRUE(fixture.profile->requested_rolling_scenarios.empty());
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, 0);
    EXPECT_EQ(fixture.profile->rolling_time_average_request_count, 0);
}

TEST(BenchmarkTracking, SelectingTrackableBenchmarkProducesProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.tracking->benchmark()->attainedRank()->value, "silver");
    EXPECT_DOUBLE_EQ(fixture.tracking->benchmark()->totalPlaytimeSeconds(), 90.0);
}

TEST(BenchmarkTracking, RequestsUniqueMappedHashesAndTheProfileIsNeverAskedForARollingSeries_R24) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized.push_back(benchmarkEntry("e3", "Again", std::string{"h1"}, rungs()));
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.size(), 2U);
    EXPECT_TRUE(fixture.profile->requested_rolling_scenarios.empty());
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, 0);
    EXPECT_EQ(fixture.profile->rolling_time_average_request_count, 0);
}

TEST(BenchmarkTracking, DoesNotRequestUnresolvedEntries) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized[1].hashes.clear();
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.profile->requested_fact_scenarios.size(), 1U);
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.front().hash, "h1");
}

TEST(BenchmarkTracking, ShowsTheRollingSeriesOfTheEvaluatedRuns_R24) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    const auto definition = benchmark("b1");
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto reference = evaluatedBenchmark(definition, facts, {resolvedEntry("e1"), mappedUnavailableEntry("e2")});
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.playtimePreferences().range = RangePreference{};
    fixture.build();
    fixture.tracking->select(definition.id);

    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    const auto expected = includeInactiveDays(reference.rollingPlaytime(5));
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, expected);
}

TEST(BenchmarkTracking, MissingProblemAndDuplicateDefinitionsAreUnavailable) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "a.json"));
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
    fixture.tracking->select(BenchmarkId{"missing"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
}

TEST(BenchmarkTracking, ProblemDefinitionIsUnavailable) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"b1"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

TEST(BenchmarkTracking, ClearAndRepeatSelectionHaveStableNotifications) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(notifications, 0);
    fixture.tracking->clearSelection();
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::NoSelection);
}

TEST(BenchmarkTracking, IncompleteBenchmarkStillProjectsSafeFacts) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    auto value = benchmark("b1");
    value.uncategorized[1].thresholds.clear();
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    EXPECT_EQ(fixture.tracking->benchmark()->completeness().completeness, Completeness::Incomplete);
    EXPECT_FALSE(fixture.tracking->benchmark()->attainedRank());
    EXPECT_DOUBLE_EQ(fixture.tracking->benchmark()->totalPlaytimeSeconds(), 60.0);
}

TEST(BenchmarkTrackingCache, ReusesPriorSelectionUntilRevisionChanges) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int first = fixture.profile->fact_request_count;
    fixture.tracking->select(BenchmarkId{"b2"});
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->fact_request_count, first + 1);
}

// The other half of the cache contract: an entry cached before a publication must not be served
// after it, or an edit would keep showing history the current definition would never produce.
TEST(BenchmarkTrackingCache, DoesNotServeAnEntryCachedBeforeALibraryChange) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    fixture.tracking->select(BenchmarkId{"b2"});

    fixture.benchmarks->refresh();
    const int afterRefresh = fixture.profile->fact_request_count;
    fixture.tracking->select(BenchmarkId{"b1"});

    EXPECT_GT(fixture.profile->fact_request_count, afterRefresh);
}

TEST(BenchmarkTrackingCache, ProfileAndLibraryChangesReevaluateAndRemovalIsUnavailable) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int selected = fixture.profile->fact_request_count;
    fixture.profile->notifyProfileChanged();
    EXPECT_GT(fixture.profile->fact_request_count, selected);
    const int changed = fixture.profile->fact_request_count;
    fixture.benchmarks->refresh();
    EXPECT_GT(fixture.profile->fact_request_count, changed);
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
}

// Resolution already reacts to both streams and republishes, so tracking listening to them as
// well would evaluate the selected benchmark twice per update.
TEST(BenchmarkTrackingCache, OneProfileChangeEvaluatesTheSelectedBenchmarkOnce) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int before = fixture.profile->fact_request_count;
    fixture.profile->notifyProfileChanged();
    EXPECT_EQ(fixture.profile->fact_request_count, before + 1);
}

TEST(BenchmarkTrackingCache, OneLibraryChangeEvaluatesTheSelectedBenchmarkOnce) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int before = fixture.profile->fact_request_count;
    fixture.benchmarks->refresh();
    EXPECT_EQ(fixture.profile->fact_request_count, before + 1);
}

TEST(BenchmarkTrackingCache, DefinitionEditReconstructsTheCachedProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 150.0F, 10.0F),
                                  benchmarkFact("h2", 101 * kDay, 150.0F, 10.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    ASSERT_TRUE(fixture.tracking->benchmark()->averageRank());
    EXPECT_DOUBLE_EQ(*fixture.tracking->benchmark()->averageRank(), 1.5);

    auto edited = benchmark("b1");
    const std::vector<std::pair<std::string, double> > halved{
        {"bronze", 50.0}, {"silver", 100.0}, {"gold", 150.0}};
    edited.uncategorized[0] = benchmarkEntry("e1", "Alpha", std::string{"h1"}, halved);
    edited.uncategorized[1] = benchmarkEntry("e2", "Beta", std::string{"h2"}, halved);
    fixture.repo->nextScan = {snapshotFor(edited), std::nullopt};
    fixture.benchmarks->refresh();

    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    ASSERT_TRUE(fixture.tracking->benchmark()->averageRank());
    EXPECT_DOUBLE_EQ(*fixture.tracking->benchmark()->averageRank(), 3.0);
    ASSERT_EQ(fixture.tracking->benchmark()->averageRankHistory().size(), 2U);
    EXPECT_DOUBLE_EQ(fixture.tracking->benchmark()->averageRankHistory().back().averageRank, 3.0);
}

// ---- Task A1: one coherent workspace snapshot ------------------------------------------------

namespace {
    BenchmarkFileEntry loadedEntry(const BenchmarkDefinition &value, const std::string &filename) {
        return {filename, "d-" + filename, LoadedBenchmark{value, validateBenchmark(value)}};
    }

    BenchmarkFileEntry problemEntry(const std::string &filename, BenchmarkFileProblem problem,
                                    std::optional<std::string> name, std::optional<BenchmarkId> id,
                                    std::optional<int> schemaVersion) {
        return {filename, "d-" + filename,
                ProblemBenchmark{problem, std::move(name), std::move(id), schemaVersion}};
    }

    BenchmarkDefinition incompleteBenchmark(const std::string &id) {
        auto value = benchmark(id);
        value.uncategorized[1].thresholds.clear();
        return value;
    }
}

TEST(BenchmarkTrackingWorkspaceSnapshot, StartupPublishesNoSelectionWithChoicesInRepositoryOrder) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(loadedEntry(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(problemEntry("broken.json", BenchmarkFileProblem::Invalid,
                                            std::string{"Broken"}, BenchmarkId{"b2"}, std::nullopt));
    snapshot.entries.push_back(loadedEntry(benchmark("b3"), "b3.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::NoSelection);
    EXPECT_FALSE(snap.selectedId.has_value());
    EXPECT_FALSE(snap.benchmark.has_value());
    EXPECT_EQ(snap.libraryRevision, fixture.benchmarks->revision());
    ASSERT_EQ(snap.choices.size(), 3U);
    EXPECT_EQ(snap.choices[0].filename, "b1.json");
    EXPECT_EQ(snap.choices[1].filename, "broken.json");
    EXPECT_EQ(snap.choices[2].filename, "b3.json");
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ChoicesCarryClassificationSelectabilityNameAndSchemaVersion) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(loadedEntry(benchmark("trackable"), "trackable.json"));
    snapshot.entries.push_back(loadedEntry(incompleteBenchmark("incomplete"), "incomplete.json"));
    snapshot.entries.push_back(problemEntry("invalid.json", BenchmarkFileProblem::Invalid,
                                            std::string{"Broken Bench"}, BenchmarkId{"bad"}, std::nullopt));
    snapshot.entries.push_back(problemEntry("legacy.json", BenchmarkFileProblem::Unsupported,
                                            std::string{"Legacy Bench"}, BenchmarkId{"old"}, 2));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &choices = fixture.tracking->snapshot().choices;
    ASSERT_EQ(choices.size(), 4U);

    EXPECT_EQ(choices[0].classification, BenchmarkChoiceClassification::Trackable);
    EXPECT_TRUE(choices[0].selectable);
    EXPECT_EQ(choices[0].displayName, "Tracked");
    ASSERT_TRUE(choices[0].id.has_value());
    EXPECT_EQ(choices[0].id->value, "trackable");
    EXPECT_FALSE(choices[0].schemaVersion.has_value());

    EXPECT_EQ(choices[1].classification, BenchmarkChoiceClassification::Incomplete);
    EXPECT_TRUE(choices[1].selectable);

    EXPECT_EQ(choices[2].classification, BenchmarkChoiceClassification::Invalid);
    EXPECT_FALSE(choices[2].selectable);
    EXPECT_EQ(choices[2].displayName, "Broken Bench");
    ASSERT_TRUE(choices[2].id.has_value());
    EXPECT_EQ(choices[2].id->value, "bad");

    EXPECT_EQ(choices[3].classification, BenchmarkChoiceClassification::Unsupported);
    EXPECT_FALSE(choices[3].selectable);
    EXPECT_EQ(choices[3].displayName, "Legacy Bench");
    ASSERT_TRUE(choices[3].schemaVersion.has_value());
    EXPECT_EQ(*choices[3].schemaVersion, 2);
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ProblemEntryWithoutParsedNameUsesFilenameAsSafeDisplayName) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(problemEntry("mystery.json", BenchmarkFileProblem::Invalid,
                                            std::nullopt, std::nullopt, std::nullopt));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &choices = fixture.tracking->snapshot().choices;
    ASSERT_EQ(choices.size(), 1U);
    EXPECT_FALSE(choices[0].id.has_value());
    EXPECT_EQ(choices[0].displayName, "mystery.json");
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ReadyTrackableSnapshotCarriesDefinitionAndProjectionInOneRevision) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();

    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    ASSERT_TRUE(snap.benchmark.has_value());
    EXPECT_EQ(snap.benchmark->completeness().completeness, Completeness::Trackable);
    EXPECT_TRUE(snap.benchmark->completeness().issues.empty());
    EXPECT_EQ(snap.benchmark->id().value, "b1");
    ASSERT_TRUE(snap.benchmark->attainedRank().has_value());
    EXPECT_EQ(snap.benchmark->attainedRank()->value, "silver");
    EXPECT_EQ(snap.libraryRevision, fixture.benchmarks->revision());
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ReadyIncompleteSnapshotCarriesFullCompletenessIssueVector) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    const auto value = incompleteBenchmark("b1");
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.build();

    fixture.tracking->select(BenchmarkId{"b1"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.benchmark.has_value());
    EXPECT_EQ(snap.benchmark->completeness().completeness, Completeness::Incomplete);
    const auto expectedIssues = validateBenchmark(value).issues;
    ASSERT_FALSE(expectedIssues.empty());
    EXPECT_EQ(snap.benchmark->completeness().issues.size(), expectedIssues.size());
}

// ---- Task A2: selection lifecycle across deletion, conflict, and recovery --------------------

TEST(BenchmarkTrackingWorkspaceLifecycle, ClearSelectionReturnsToNoSelectionAndDropsDerivedState) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.tracking->clearSelection();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::NoSelection);
    EXPECT_FALSE(snap.selectedId.has_value());
    EXPECT_FALSE(snap.benchmark.has_value());
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, SelectingUnknownIdPublishesUnavailableWithoutARetainedProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_TRUE(fixture.tracking->snapshot().benchmark.has_value());

    fixture.tracking->select(BenchmarkId{"ghost"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    EXPECT_FALSE(snap.benchmark.has_value());
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

// The selected file is removed entirely on the next scan. The workspace must keep the selected id
// and the last name the user saw, but drop every derived field, so the Unavailable screen can name
// the benchmark that vanished without showing stale ranks or history.
TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedDeletionGoesUnavailableRetainingIdAndLastKnownName) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);
    ASSERT_EQ(fixture.tracking->snapshot().lastKnownSelectedDisplayName, "Tracked");

    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.benchmark.has_value());
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

// The file survives but now fails to parse and exposes no embedded id, so it never appears as a
// choice the current lookup can read a name from. The last-known name must still be retained.
TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedBecomingUnparseableProblemRetainsLastKnownName) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    BenchmarkLibrarySnapshot broken;
    broken.entries.push_back({"b1.json", "digest",
                              ProblemBenchmark{BenchmarkFileProblem::Invalid, std::nullopt,
                                               std::nullopt, std::nullopt}});
    fixture.repo->nextScan = {broken, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.benchmark.has_value());
}

TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedDuplicateIdConflictGoesUnavailableAndClearsDerivedState) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    BenchmarkLibrarySnapshot clash;
    clash.entries.push_back(loadedEntry(benchmark("b1"), "a.json"));
    clash.entries.push_back(loadedEntry(benchmark("b1"), "b.json"));
    fixture.repo->nextScan = {clash, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.benchmark.has_value());
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

// A directory-level scan failure is not a deletion: the library keeps its last accepted snapshot,
// so the tracking workspace must keep showing the selected benchmark rather than dropping to Unavailable.
TEST(BenchmarkTrackingWorkspaceLifecycle, WholeDirectoryRefreshFailureKeepsAcceptedProjectionVisible) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    fixture.benchmarks->refresh();

    ASSERT_TRUE(fixture.benchmarks->lastRefreshFailed());
    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.benchmark.has_value());
    EXPECT_EQ(snap.benchmark->id().value, "b1");
    EXPECT_NE(fixture.tracking->benchmark(), nullptr);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, ProfileChangeInvalidatesProjectionButKeepsChoicesAndSelection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const auto choicesBefore = fixture.tracking->snapshot().choices.size();
    const auto selectedBefore = fixture.tracking->snapshot().selectedId;
    const int factsBefore = fixture.profile->fact_request_count;

    fixture.profile->notifyProfileChanged();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_GT(fixture.profile->fact_request_count, factsBefore);
    EXPECT_EQ(snap.choices.size(), choicesBefore);
    EXPECT_EQ(snap.selectedId, selectedBefore);
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, UnavailableSelectionAutoRecoversWhenSameIdReturns) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Unavailable);

    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    ASSERT_TRUE(snap.benchmark.has_value());
    EXPECT_EQ(snap.benchmark->id().value, "b1");
}

// ---- default selection and remembered choice ------------------------------------------

namespace {
    struct RememberingFixture {
        std::shared_ptr<FakeBenchmarkStore> repo = std::make_shared<FakeBenchmarkStore>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        std::shared_ptr<BenchmarksService> benchmarks;
        std::shared_ptr<BenchmarkResolutionUseCase> resolution;
        std::shared_ptr<RecentBenchmarks> recent;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        void build(const std::vector<std::string> &ids) {
            BenchmarkLibrarySnapshot snapshot;
            for (const auto &id: ids) snapshot.entries.push_back(fileFor(benchmark(id), id + ".json"));
            repo->nextScan = {snapshot, std::nullopt};
            benchmarks = std::make_shared<BenchmarksService>(repo);
            resolution = std::make_shared<BenchmarkResolutionUseCase>(benchmarks, profile);
            recent = std::make_shared<RecentBenchmarks>(settings);
            tracking = std::make_unique<BenchmarkTrackingUseCase>(benchmarks, resolution, profile, configuration, days,
                                                                  recent);
        }

        std::optional<std::size_t> recencyOf(const std::string &id) const {
            for (const auto &choice: tracking->snapshot().choices)
                if (choice.id && choice.id->value == id) return choice.recency;
            return std::nullopt;
        }
    };

    using Ids = std::vector<std::string>;
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ACachedEvaluationIsPublishedAsReadyEvenWithoutALoadedEntry) {
    RememberingFixture fixture;
    const auto definition = benchmark("b1");
    fixture.profile->scenarios = {{"Alpha", "h1"}, {"Beta", "h2"}};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.benchmarks = std::make_shared<BenchmarksService>(fixture.repo);
    fixture.recent = std::make_shared<RecentBenchmarks>(fixture.settings);
    // Notify tracking of the recents change before resolution handles the library change.
    fixture.benchmarks->onChanged([&] { fixture.recent->markCreated("b2"); });
    fixture.resolution = std::make_shared<BenchmarkResolutionUseCase>(fixture.benchmarks, fixture.profile);
    fixture.tracking = std::make_unique<BenchmarkTrackingUseCase>(fixture.benchmarks, fixture.resolution,
                                                                 fixture.profile, fixture.configuration,
                                                                 fixture.days, fixture.recent);
    fixture.tracking->select(definition.id);
    const auto factRequests = fixture.profile->fact_request_count;
    const auto expected = evaluatedBenchmark(definition, fixture.profile->run_facts);

    int readyRevisions = 0;
    fixture.tracking->onChanged([&] {
        const auto &snapshot = fixture.tracking->snapshot();
        if (snapshot.availability != BenchmarkTrackingState::Ready) return;
        ++readyRevisions;
        EXPECT_TRUE(snapshot.choices.empty());
        ASSERT_TRUE(snapshot.benchmark);
        EXPECT_EQ(fixture.tracking->benchmark(), &*snapshot.benchmark);
        EXPECT_EQ(snapshot.benchmark->definition(), definition);
        expectSameBenchmark(*snapshot.benchmark, expected, snapshot.playtime.windowDays);
        EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    });
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();

    EXPECT_EQ(readyRevisions, 1);
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(fixture.tracking->benchmark(), nullptr);
}

TEST(BenchmarkTrackingDefaultSelection, NothingIsSelectedUntilAskedEvenWithASingleBenchmark) {
    RememberingFixture fixture;
    fixture.build({"only"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::NoSelection);
}

TEST(BenchmarkTrackingDefaultSelection, SelectsTheOnlyBenchmark) {
    RememberingFixture fixture;
    fixture.build({"only"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
    EXPECT_EQ(notifications, 1);
}

TEST(BenchmarkTrackingDefaultSelection, DoesNotGuessBetweenSeveralBenchmarks) {
    RememberingFixture fixture;
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    EXPECT_FALSE(fixture.tracking->selected());
    EXPECT_EQ(notifications, 0);
}

TEST(BenchmarkTrackingDefaultSelection, IgnoresProblemFilesWhenCountingTheOnlyBenchmark) {
    RememberingFixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("good"), "good.json"));
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"bad"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.benchmarks = std::make_shared<BenchmarksService>(fixture.repo);
    fixture.resolution = std::make_shared<BenchmarkResolutionUseCase>(fixture.benchmarks, fixture.profile);
    fixture.recent = std::make_shared<RecentBenchmarks>(fixture.settings);
    fixture.tracking = std::make_unique<BenchmarkTrackingUseCase>(fixture.benchmarks, fixture.resolution,
                                                                  fixture.profile, fixture.configuration,
                                                                  fixture.days, fixture.recent);

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "good");
}

TEST(BenchmarkTrackingDefaultSelection, RestoresTheRememberedBenchmarkAmongSeveral) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b2"};
    fixture.build({"b1", "b2", "b3"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b2");
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
}

TEST(BenchmarkTrackingDefaultSelection, FallsBackToTheOnlyBenchmarkWhenTheRememberedOneIsGone) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted"};
    fixture.build({"only"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
}

TEST(BenchmarkTrackingDefaultSelection, RememberedButMissingAmongSeveralSelectsNothing) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted"};
    fixture.build({"b1", "b2"});

    fixture.tracking->selectDefault();

    EXPECT_FALSE(fixture.tracking->selected());
}

TEST(BenchmarkTrackingDefaultSelection, NeverReplacesAnExistingSelection) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1"};
    fixture.build({"b1", "b2"});
    fixture.tracking->select(BenchmarkId{"b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    EXPECT_EQ(fixture.tracking->selected()->value, "b2");
    EXPECT_EQ(notifications, 0);
}

TEST(BenchmarkTrackingDefaultSelection, SkipsAFrontIdThatIsNoLongerSelectableAndTakesTheNext) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted", "b3", "b1"};
    fixture.build({"b1", "b2", "b3"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b3");
}

TEST(BenchmarkTrackingDefaultSelection, SkipsARecentIdThatNamesAProblemFile) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"bad", "b1"};
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"bad"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.benchmarks = std::make_shared<BenchmarksService>(fixture.repo);
    fixture.resolution = std::make_shared<BenchmarkResolutionUseCase>(fixture.benchmarks, fixture.profile);
    fixture.recent = std::make_shared<RecentBenchmarks>(fixture.settings);
    fixture.tracking = std::make_unique<BenchmarkTrackingUseCase>(fixture.benchmarks, fixture.resolution,
                                                                  fixture.profile, fixture.configuration,
                                                                  fixture.days, fixture.recent);

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b1");
}

TEST(BenchmarkTrackingRecent, SelectMovesItsIdToTheFrontAndNotifiesExactlyOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1", "b2"};
    fixture.build({"b1", "b2", "b3"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->select(BenchmarkId{"b3"});

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b3", "b1", "b2"}));
    EXPECT_EQ(fixture.recencyOf("b3"), 0U);
    EXPECT_EQ(fixture.recencyOf("b1"), 1U);
    EXPECT_EQ(fixture.recencyOf("b2"), 2U);
}

TEST(BenchmarkTrackingRecent, SelectingAnAlreadyFrontIdNotifiesOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b2", "b1"};
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->select(BenchmarkId{"b2"});

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b2", "b1"}));
}

TEST(BenchmarkTrackingRecent, ClearSelectionLeavesTheListUnchanged) {
    RememberingFixture fixture;
    fixture.build({"b1", "b2"});
    fixture.tracking->select(BenchmarkId{"b2"});
    const int writes = fixture.settings->recentBenchmarkIdsWrites;

    fixture.tracking->clearSelection();

    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b2"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, writes);
    EXPECT_EQ(fixture.recencyOf("b2"), 0U);
}

TEST(BenchmarkTrackingRecent, ChoicesCarryTheirIndexInTheListOrNothing) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"gone", "b3", "b1"};
    fixture.build({"b1", "b2", "b3"});

    EXPECT_EQ(fixture.recencyOf("b3"), 1U);
    EXPECT_EQ(fixture.recencyOf("b1"), 2U);
    EXPECT_EQ(fixture.recencyOf("b2"), std::nullopt);
}

TEST(BenchmarkTrackingRecent, ACreateMadeElsewhereReordersTheChoicesAndNotifiesOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1"};
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.recent->markCreated("b2");

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.recencyOf("b1"), 0U);
    EXPECT_EQ(fixture.recencyOf("b2"), 1U);
}

TEST(BenchmarkTrackingRecent, WithoutAListChoicesHaveNoRecency) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});

    ASSERT_EQ(fixture.tracking->snapshot().choices.size(), 1U);
    EXPECT_FALSE(fixture.tracking->snapshot().choices.front().recency);
}

// The restart round trip: a choice made in one session is what the next session restores.
TEST(BenchmarkTrackingDefaultSelection, ChoiceSurvivesIntoANewSession) {
    RememberingFixture first;
    first.build({"b1", "b2", "b3"});
    first.tracking->select(BenchmarkId{"b3"});

    RememberingFixture second;
    second.settings = first.settings;
    second.build({"b1", "b2", "b3"});
    EXPECT_FALSE(second.tracking->selected()) << "nothing is selected before the workspace asks";
    second.tracking->selectDefault();

    ASSERT_TRUE(second.tracking->selected());
    EXPECT_EQ(second.tracking->selected()->value, "b3");
}

TEST(BenchmarkTrackingDefaultSelection, WithoutSettingsOnlyTheSoleBenchmarkRuleApplies) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("only")), std::nullopt};
    fixture.build();

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
}

TEST(BenchmarkTracking, RequestsEveryExplicitLinkedHashOnceAndTheProfileIsNeverAskedForARollingSeries_R24) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized[0].hashes = {"h1", "new", "h2"};
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.size(), 3U);
    EXPECT_TRUE(fixture.profile->requested_rolling_scenarios.empty());
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, 0);
    EXPECT_EQ(fixture.profile->rolling_time_average_request_count, 0);
    EXPECT_TRUE(std::ranges::any_of(fixture.profile->requested_fact_scenarios,
                                  [](const auto &scenario) { return scenario.hash == "new"; }));
}

// ---- playtime window and range ----------------------------------------------------------

namespace {
    using Series = std::vector<std::pair<std::chrono::sys_days, double> >;

    std::chrono::sys_days epochDay(const long long day) { return std::chrono::sys_days{std::chrono::days{day}}; }

    void expectSameSummary(const Benchmark &left, const Benchmark &right) {
        EXPECT_EQ(left.attainedRank(), right.attainedRank());
        EXPECT_EQ(left.nextTier(), right.nextTier());
        EXPECT_EQ(left.averageRank(), right.averageRank());
        EXPECT_DOUBLE_EQ(left.totalPlaytimeSeconds(), right.totalPlaytimeSeconds());
        const auto leftScenarios = left.scenarios();
        const auto rightScenarios = right.scenarios();
        ASSERT_EQ(leftScenarios.size(), rightScenarios.size());
        for (std::size_t index = 0; index < leftScenarios.size(); ++index) {
            EXPECT_EQ(leftScenarios[index]->stats(), rightScenarios[index]->stats());
            EXPECT_EQ(leftScenarios[index]->attainedTier(), rightScenarios[index]->attainedTier());
        }
        const auto &leftHistory = left.averageRankHistory();
        const auto &rightHistory = right.averageRankHistory();
        ASSERT_EQ(leftHistory.size(), rightHistory.size());
        for (std::size_t index = 0; index < leftHistory.size(); ++index) {
            EXPECT_EQ(leftHistory[index].day, rightHistory[index].day);
            EXPECT_EQ(leftHistory[index].averageRank, rightHistory[index].averageRank);
        }
    }

    struct PlaytimeFixture : Fixture {
        int tracked = 0;
        int playtimeOnly = 0;

        PlaytimeFixture() {
            known("Alpha", "h1");
            known("Beta", "h2");
            repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
        }

        void start() {
            build();
            tracking->select(BenchmarkId{"b1"});
            tracking->onChanged([this] { ++tracked; });
            tracking->onPlaytimeChanged([this] { ++playtimeOnly; });
        }
    };
}

TEST(BenchmarkTrackingPlaytime, VisiblePlaytimeIsTheRangeSlicedSeriesOfTheCommittedWindow) {
    PlaytimeFixture fixture;
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto reference = evaluatedBenchmark(benchmark("b1"), facts);
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    const auto full = includeInactiveDays(reference.rollingPlaytime(5));
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.playtimePreferences().range = RangePreference{AllHistory{}, false};
    fixture.days->setToday(epochDay(106));
    fixture.start();

    const auto &snapshot = fixture.tracking->snapshot();
    ASSERT_TRUE(snapshot.benchmark);
    EXPECT_EQ(snapshot.playtime.windowDays, 5);
    EXPECT_EQ(snapshot.visiblePlaytime, full);

    const RangePreference narrowed{RecentDays{3}, false};
    fixture.playtimePreferences().range = narrowed;
    fixture.commitPlaytime();

    const auto range = resolveRange(narrowed, fixture.days->today(), reference.latestRunDay());
    EXPECT_EQ(snapshot.visiblePlaytime, visiblePlaytime(full, range));
    EXPECT_EQ(snapshot.playtime.windowDays, 5);
}

TEST(BenchmarkTrackingPlaytime, AWindowOrRangeChangeNeitherRebuildsTheBenchmarkNorReadsProfileFacts) {
    PlaytimeFixture fixture;
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto reference = evaluatedBenchmark(benchmark("b1"), facts);
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    fixture.playtimePreferences().range = RangePreference{AllHistory{}, false};
    fixture.days->setToday(epochDay(106));
    fixture.start();
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    const auto before = *fixture.tracking->benchmark();
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.commitPlaytime();
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    expectSameBenchmark(*fixture.tracking->benchmark(), before, 5);
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, includeInactiveDays(reference.rollingPlaytime(5)));
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 1);

    fixture.playtimePreferences().range = RangePreference{RecentDays{3}, false};
    fixture.commitPlaytime();
    ASSERT_NE(fixture.tracking->benchmark(), nullptr);
    expectSameBenchmark(*fixture.tracking->benchmark(), before, 5);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 2);
}

TEST(BenchmarkTrackingPlaytime, PlaytimeCountsASharedHashOncePerResolvedEntryNotOncePerHash_R28) {
    PlaytimeFixture fixture;
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    auto definition = benchmark("b1");
    definition.uncategorized.pop_back();
    const auto single = evaluatedBenchmark(definition, facts);
    definition.uncategorized.push_back(benchmarkEntry("e3", "Again", std::string{"h1"}, rungs()));
    const auto reference = evaluatedBenchmark(definition, facts);
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    const auto full = includeInactiveDays(reference.rollingPlaytime(5));
    ASSERT_NE(full, includeInactiveDays(single.rollingPlaytime(5)));
    fixture.repo->nextScan = {snapshotFor(definition), std::nullopt};
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.playtimePreferences().range = RangePreference{AllHistory{}, false};
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, full);
}

TEST(BenchmarkTrackingPlaytime, PlaytimeAndAnchorIgnoreRunsOfAMappedButUnavailableEntry_R28) {
    PlaytimeFixture fixture;
    fixture.profile->scenarios.pop_back();
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto definition = benchmark("b1");
    const auto reference = evaluatedBenchmark(definition, facts,
                                              {resolvedEntry("e1"), mappedUnavailableEntry("e2")});
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    const RangePreference anchored{RecentDays{3}, true};
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.playtimePreferences().range = anchored;
    fixture.days->setToday(epochDay(150));
    fixture.start();

    const auto range = resolveRange(anchored, fixture.days->today(), reference.latestRunDay());
    const auto full = includeInactiveDays(reference.rollingPlaytime(5));
    const auto expected = visiblePlaytime(full, range);
    ASSERT_FALSE(expected.empty());
    const auto interval = describePlaytimeView(5, range, full).interval;
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, expected);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval, interval);

    fixture.profile->run_facts.push_back(benchmarkFact("h2", 120 * kDay, 300.0F, 120.0F));
    fixture.profile->notifyProfileChanged();

    const auto withUnavailable = evaluatedBenchmark(definition, fixture.profile->run_facts,
                                                    {resolvedEntry("e1"), mappedUnavailableEntry("e2")});
    EXPECT_EQ(withUnavailable.rollingPlaytime(5), reference.rollingPlaytime(5));
    EXPECT_EQ(withUnavailable.latestRunDay(), reference.latestRunDay());
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, expected);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval, interval);
}

TEST(BenchmarkTrackingPlaytime, UsesTheCommittedWindowForTheSeries_R24) {
    PlaytimeFixture fixture;
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto reference = evaluatedBenchmark(benchmark("b1"), facts);
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, includeInactiveDays(reference.rollingPlaytime(5)));
    EXPECT_EQ(fixture.tracking->snapshot().playtime.windowDays, 5);
}

TEST(BenchmarkTrackingPlaytime, OneDayWindowIncludesZeroTotalsForDaysWithoutRuns) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.playtimePreferences().averagingWindowDays = 1;
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime,
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(102), 0.0},
                      {epochDay(103), 0.0}, {epochDay(104), 90.0}}));
}

TEST(BenchmarkTrackingPlaytime, RollingWindowIncludesZeroDaysAfterEarlierPlaytimeLeavesTheWindow) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {106, 90.0F}});
    fixture.playtimePreferences().averagingWindowDays = 3;
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime,
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(102), 20.0},
                      {epochDay(103), 0.0}, {epochDay(104), 0.0}, {epochDay(105), 0.0},
                      {epochDay(106), 30.0}}));
}

TEST(BenchmarkTrackingPlaytime, CustomRangeRetainsZeroDaysWithinTheHistoryGap) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {106, 90.0F}});
    fixture.playtimePreferences().averagingWindowDays = 3;
    fixture.start();
    const auto before = *fixture.tracking->benchmark();
    fixture.playtimePreferences().range = RangePreference{CustomDates{epochDay(102), epochDay(105)}, false};
    fixture.commitPlaytime();

    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime,
              (Series{{epochDay(102), 20.0}, {epochDay(103), 0.0},
                      {epochDay(104), 0.0}, {epochDay(105), 0.0}}));
    expectSameSummary(before, *fixture.tracking->benchmark());
}

TEST(BenchmarkTrackingPlaytime, ChangingTheWindowRecalculatesOnlyPlaytimeAndNeverAsksTheProfileForASeries_R24) {
    PlaytimeFixture fixture;
    const auto facts = useRealAccounting(fixture, kUnequalDailyRuns);
    const auto reference = evaluatedBenchmark(benchmark("b1"), facts);
    ASSERT_NO_FATAL_FAILURE(expectDistinctWindows(reference, 5));
    fixture.playtimePreferences().averagingWindowDays = 3;
    fixture.start();
    const auto before = *fixture.tracking->benchmark();
    const auto beforePlaytime = fixture.tracking->snapshot().visiblePlaytime;
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.commitPlaytime();

    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, 0);
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, includeInactiveDays(reference.rollingPlaytime(5)));
    EXPECT_NE(fixture.tracking->snapshot().visiblePlaytime, beforePlaytime);
    expectSameSummary(before, *fixture.tracking->benchmark());
}

TEST(BenchmarkTrackingPlaytime, ANarrowerRangeKeepsOverlappingValuesAndTheWindow) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 30.0F}});
    fixture.playtimePreferences().averagingWindowDays = 4;
    fixture.start();
    const auto before = *fixture.tracking->benchmark();
    const auto beforePlaytime = fixture.tracking->snapshot().visiblePlaytime;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;

    fixture.days->setToday(epochDay(106));
    fixture.playtimePreferences().range = RangePreference{RecentDays{3}, false};
    fixture.commitPlaytime();

    const auto &narrowed = fixture.tracking->snapshot().visiblePlaytime;
    ASSERT_EQ(narrowed.size(), 3U);
    EXPECT_EQ(narrowed, Series(beforePlaytime.end() - 3, beforePlaytime.end()));
    EXPECT_EQ(fixture.tracking->snapshot().playtime.windowDays, 4);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(104), epochDay(106)}));
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    expectSameSummary(before, *fixture.tracking->benchmark());
}

TEST(BenchmarkTrackingPlaytime, TheFullExtentSurvivesNarrowing) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.start();
    const auto fullExtent = fixture.tracking->snapshot().playtime.fullExtent;
    ASSERT_TRUE(fullExtent);

    fixture.playtimePreferences().range = RangePreference{RecentDays{1}, true};
    fixture.commitPlaytime();

    EXPECT_EQ(fixture.tracking->snapshot().playtime.fullExtent, fullExtent);
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime.size(), 1U);
}

TEST(BenchmarkTrackingPlaytime, TheAnchorIsTheNewestContributingRunEvenWhenItImprovedNothing) {
    PlaytimeFixture fixture;
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F),
                                  benchmarkFact("h1", 110 * kDay, 10.0F, 30.0F)};
    fixture.days->setToday(epochDay(400));
    fixture.playtimePreferences().range = RangePreference{RecentDays{5}, true};
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(106), epochDay(110)}));
}

TEST(BenchmarkTrackingPlaytime, AnchoredRangeWithoutContributingRunsHasNoIntervalAndNoSamples) {
    PlaytimeFixture fixture;
    fixture.days->setToday(epochDay(400));
    fixture.playtimePreferences().range = RangePreference{RecentDays{5}, true};
    fixture.start();

    EXPECT_FALSE(fixture.tracking->snapshot().playtime.interval);
    EXPECT_TRUE(fixture.tracking->snapshot().visiblePlaytime.empty());
}

TEST(BenchmarkTrackingPlaytime, ADayChangeReprojectsWithoutRecalculatingOrTouchingTheSummaries) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}});
    fixture.days->setToday(epochDay(101));
    fixture.playtimePreferences().range = RangePreference{RecentDays{2}, false};
    fixture.start();
    const auto before = *fixture.tracking->benchmark();
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.days->advance();

    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(101), epochDay(102)}));
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime.size(), 1U);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    expectSameSummary(before, *fixture.tracking->benchmark());
}

TEST(BenchmarkTrackingPlaytime, ADayChangeIsIgnoredWhenTheRangeDoesNotFollowToday) {
    PlaytimeFixture fixture;
    fixture.start();

    fixture.days->advance();

    EXPECT_EQ(fixture.playtimeOnly, 0);
}

TEST(BenchmarkTrackingPlaytime, OtherGraphsAndUnselectedStatesAreNotRefreshed) {
    PlaytimeFixture fixture;
    fixture.start();

    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    fixture.commitPlaytime(GraphKey::DailyPlaytime);
    EXPECT_EQ(fixture.playtimeOnly, 0);

    fixture.tracking->clearSelection();
    fixture.commitPlaytime();
    EXPECT_EQ(fixture.playtimeOnly, 0);
}

// ---- rank range ---------------------------------------------------------------------------

namespace {
    struct RankFixture : PlaytimeFixture {
        int rankOnly = 0;
        int recentOnly = 0;

        GraphPreferences &rankPreferences() {
            return configuration->configuration.preferences(GraphKey::BenchmarkRank);
        }

        void startRank() {
            start();
            tracking->onRankChanged([this](const GraphKey graph) {
                ++(graph == GraphKey::BenchmarkRecentRank ? recentOnly : rankOnly);
            });
        }

        void improvingThenFlatRuns() {
            profile->run_facts = {benchmarkFact("h1", 100 * kDay, 150.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 250.0F, 30.0F),
                                  benchmarkFact("h1", 104 * kDay, 260.0F, 30.0F),
                                  benchmarkFact("h1", 110 * kDay, 10.0F, 30.0F)};
        }
    };
}

TEST(BenchmarkTrackingRank, AllHistoryLeavesTheRankRangeUnrestricted) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{};
    fixture.startRank();

    EXPECT_EQ(fixture.tracking->snapshot().rank, RankRangeView{});
    EXPECT_EQ(fixture.tracking->benchmark()->averageRankHistory().size(), 3U);
}

TEST(BenchmarkTrackingRank, ARangeChangeReprojectsWithoutReevaluatingOrTouchingTheSummaries) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.startRank();
    const auto before = *fixture.tracking->benchmark();
    const auto factRequests = fixture.profile->fact_request_count;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;

    fixture.rankPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(102)}, std::chrono::year_month_day{epochDay(106)}}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRank);

    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(106)}));
    EXPECT_FALSE(fixture.tracking->snapshot().rank.noRelevantHistory);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    // The rank history stays whole: what came before the range is the rank carried into it.
    expectSameSummary(before, *fixture.tracking->benchmark());
}

// The rank series has no point after its last improvement, so anchoring at a later run would only
// stretch the held level past the end of the data.
TEST(BenchmarkTrackingRank, TheAnchorIsTheLatestImprovementNotTheNewestContributingRun) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    ASSERT_EQ(fixture.tracking->benchmark()->averageRankHistory().back().day, epochDay(104));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(104)}));
}

TEST(BenchmarkTrackingRank, ALaterNonImprovingRunLeavesTheAnchor) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    fixture.profile->run_facts.push_back(benchmarkFact("h2", 120 * kDay, 5.0F, 30.0F));
    fixture.profile->notifyProfileChanged();

    EXPECT_EQ(fixture.tracking->benchmark()->averageRankHistory().back().day, epochDay(104));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(104)}));
}

TEST(BenchmarkTrackingRank, ALaterImprovingRunMovesTheAnchor) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    fixture.profile->run_facts.push_back(benchmarkFact("h2", 120 * kDay, 900.0F, 30.0F));
    fixture.profile->notifyProfileChanged();

    ASSERT_EQ(fixture.tracking->benchmark()->averageRankHistory().back().day, epochDay(120));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(118), epochDay(120)}));
}

TEST(BenchmarkTrackingRank, AnchoredCustomDatesPastTheLastImprovementEndAtIt) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(102)}, std::chrono::year_month_day{epochDay(108)}}, true};
    fixture.startRank();

    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(104)}));
    EXPECT_EQ(fixture.tracking->snapshot().rank.anchoredEnd, std::optional(epochDay(104)));
}

TEST(BenchmarkTrackingRank, AnchoredCustomDatesWhollyAfterTheLastImprovementStayAsEntered) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(106)}, std::chrono::year_month_day{epochDay(108)}}, true};
    fixture.startRank();

    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(106), epochDay(108)}));
}

TEST(BenchmarkTrackingRank, UnanchoredCustomDatesStayAsEntered) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(102)}, std::chrono::year_month_day{epochDay(108)}}, false};
    fixture.startRank();

    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(108)}));
    EXPECT_FALSE(fixture.tracking->snapshot().rank.anchoredEnd);
}

TEST(BenchmarkTrackingRank, AnAnchoredWholeHistoryReportsTheLastImprovementWithoutAnInterval) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{AllHistory{}, true};
    fixture.startRank();

    EXPECT_FALSE(fixture.tracking->snapshot().rank.interval);
    EXPECT_EQ(fixture.tracking->snapshot().rank.anchoredEnd, std::optional(epochDay(104)));
}

TEST(BenchmarkTrackingRank, AnAnchoredRangeWithoutContributingRunsHasNoRelevantHistory) {
    RankFixture fixture;
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, true};
    fixture.startRank();

    EXPECT_TRUE(fixture.tracking->snapshot().rank.noRelevantHistory);
    EXPECT_FALSE(fixture.tracking->snapshot().rank.interval);
}

TEST(BenchmarkTrackingRank, ATodayAnchoredRangeFollowsADayChangeWithoutReevaluating) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(110));
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, false};
    fixture.startRank();
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.days->advance();

    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(107), epochDay(111)}));
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
}

TEST(BenchmarkTrackingRank, ADayChangeIsIgnoredWhenTheRankRangeDoesNotFollowToday) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, true};
    fixture.startRank();

    fixture.days->advance();

    EXPECT_EQ(fixture.rankOnly, 0);
}

TEST(BenchmarkTrackingRank, RankAndPlaytimeRangesNeverWriteEachOther) {
    RankFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 30.0F}});
    fixture.days->setToday(epochDay(106));
    fixture.startRank();
    const auto playtime = fixture.tracking->snapshot().visiblePlaytime;
    const auto playtimeInterval = fixture.tracking->snapshot().playtime.interval;

    fixture.rankPreferences().range = RangePreference{RecentDays{2}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval, playtimeInterval);
    EXPECT_EQ(fixture.tracking->snapshot().visiblePlaytime, playtime);

    fixture.playtimePreferences().range = RangePreference{RecentDays{4}, false};
    fixture.commitPlaytime();
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(105), epochDay(106)}));
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval, std::optional(DayInterval{epochDay(103), epochDay(106)}));
}

TEST(BenchmarkTrackingRank, OtherGraphsAndUnselectedStatesAreNotRefreshed) {
    RankFixture fixture;
    fixture.startRank();

    fixture.commitPlaytime(GraphKey::BenchmarkPlaytime);
    fixture.commitPlaytime(GraphKey::ScenarioHistory);
    EXPECT_EQ(fixture.rankOnly, 0);

    fixture.tracking->clearSelection();
    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    EXPECT_EQ(fixture.rankOnly, 0);
}

// ---- recent rank range --------------------------------------------------------------------

namespace {
    struct RecentRankFixture : RankFixture {
        GraphPreferences &recentPreferences() {
            return configuration->configuration.preferences(GraphKey::BenchmarkRecentRank);
        }
    };
}

TEST(BenchmarkTrackingRecentRank, ARangeChangeMovesOnlyTheRecentRankView) {
    RecentRankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.startRank();
    const auto rank = fixture.tracking->snapshot().rank;
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.recentPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(102)}, std::chrono::year_month_day{epochDay(106)}}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRecentRank);

    EXPECT_EQ(fixture.recentOnly, 1);
    EXPECT_EQ(fixture.rankOnly, 0);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().recentRank.interval,
              std::optional(DayInterval{epochDay(102), epochDay(106)}));
    EXPECT_EQ(fixture.tracking->snapshot().rank, rank);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
}

TEST(BenchmarkTrackingRecentRank, ARankRangeChangeLeavesTheRecentRankViewAlone) {
    RecentRankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.startRank();
    const auto recent = fixture.tracking->snapshot().recentRank;

    fixture.rankPreferences().range = RangePreference{RecentDays{2}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRank);

    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.recentOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().recentRank, recent);
}

// The recent line has a point on every play day, so it is anchored at the last one even when that
// day improved nothing and the rank graph's anchor stays earlier.
TEST(BenchmarkTrackingRecentRank, TheAnchorIsTheLastPlayDayNotTheLastImprovement) {
    RecentRankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.recentPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    ASSERT_EQ(fixture.tracking->benchmark()->recentAverageRankHistory().back().day, epochDay(110));
    EXPECT_EQ(fixture.tracking->snapshot().recentRank.interval,
              std::optional(DayInterval{epochDay(108), epochDay(110)}));
    EXPECT_EQ(fixture.tracking->snapshot().recentRank.anchoredEnd, std::optional(epochDay(110)));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(104)}));
}

TEST(BenchmarkTrackingRecentRank, AnAnchoredRangeWithoutContributingRunsHasNoRelevantHistory) {
    RecentRankFixture fixture;
    fixture.days->setToday(epochDay(400));
    fixture.recentPreferences().range = RangePreference{RecentDays{5}, true};
    fixture.startRank();

    EXPECT_TRUE(fixture.tracking->snapshot().recentRank.noRelevantHistory);
    EXPECT_FALSE(fixture.tracking->snapshot().recentRank.interval);
}

TEST(BenchmarkTrackingRecentRank, ATodayAnchoredRangeFollowsADayChangeAlone) {
    RecentRankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(110));
    fixture.recentPreferences().range = RangePreference{RecentDays{5}, false};
    fixture.startRank();

    fixture.days->advance();

    EXPECT_EQ(fixture.recentOnly, 1);
    EXPECT_EQ(fixture.rankOnly, 0);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.tracking->snapshot().recentRank.interval,
              std::optional(DayInterval{epochDay(107), epochDay(111)}));
}

TEST(BenchmarkTrackingRecentRank, UnselectedStatesAreNotRefreshed) {
    RecentRankFixture fixture;
    fixture.startRank();
    fixture.tracking->clearSelection();
    fixture.commitPlaytime(GraphKey::BenchmarkRecentRank);
    EXPECT_EQ(fixture.recentOnly, 0);
}

// ---- committed style ----------------------------------------------------------------------

namespace {
    struct StyleFixture : RankFixture {
        int style = 0;

        void startStyle() {
            startRank();
            tracking->onPresentationChanged([this] { ++style; });
        }

        void publishStyleEdits() {
            ++configuration->revision;
            configuration->publishDiff();
        }
    };
}

TEST(BenchmarkTrackingStyle, AStyleOnlyChangeNeverEvaluatesOrReprojectsEitherBenchmarkGraph) {
    StyleFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.startStyle();
    const auto factRequests = fixture.profile->fact_request_count;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;
    const auto before = *fixture.tracking->benchmark();

    fixture.playtimePreferences().appearance = {ConnectionMode::PointsOnly, true};
    fixture.playtimePreferences().seriesStyles["playtime"] = {RgbaColor{9, 9, 9, 255}, 8.0};
    fixture.rankPreferences().appearance.connection = ConnectionMode::Straight;
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{200, 0, 0, 255}, std::nullopt};
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.rankOnly, 0);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    expectSameSummary(before, *fixture.tracking->benchmark());
}

TEST(BenchmarkTrackingStyle, EachGraphReportsItsOwnCommittedStyle) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.playtimePreferences().appearance.connection = ConnectionMode::PointsOnly;
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{1, 1, 1, 255}, 5.0};
    fixture.publishStyleEdits();

    const auto rank = fixture.tracking->presentation(GraphKey::BenchmarkRank);
    const auto playtime = fixture.tracking->presentation(GraphKey::BenchmarkPlaytime);
    EXPECT_EQ(rank.appearance.connection, defaultGraphPreferences(GraphKey::BenchmarkRank).appearance.connection);
    EXPECT_EQ(rank.seriesStyle("rank"), (SeriesStyle{RgbaColor{1, 1, 1, 255}, 5.0}));
    EXPECT_EQ(playtime.appearance.connection, ConnectionMode::PointsOnly);
    EXPECT_EQ(playtime.seriesStyle("playtime").color,
              defaultSeriesStyle(GraphKey::BenchmarkPlaytime, "playtime").color);
}

TEST(BenchmarkTrackingStyle, TheRecentRankGraphsStyleIsAnnouncedOnceAndReportedOnItsOwn) {
    StyleFixture fixture;
    fixture.startStyle();

    auto &recent = fixture.configuration->configuration.preferences(GraphKey::BenchmarkRecentRank);
    recent.appearance.connection = ConnectionMode::PointsOnly;
    recent.seriesStyles["rank"] = {RgbaColor{7, 7, 7, 255}, 2.0};
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 1);
    EXPECT_EQ(fixture.rankOnly, 0);
    const auto presentation = fixture.tracking->presentation(GraphKey::BenchmarkRecentRank);
    EXPECT_EQ(presentation.appearance.connection, ConnectionMode::PointsOnly);
    EXPECT_EQ(presentation.seriesStyle("rank"), (SeriesStyle{RgbaColor{7, 7, 7, 255}, 2.0}));
    EXPECT_EQ(fixture.tracking->presentation(GraphKey::BenchmarkRank).seriesStyle("rank"),
              defaultSeriesStyle(GraphKey::BenchmarkRank, "rank"));
}

TEST(BenchmarkTrackingStyle, RangeAndWindowChangesTakeTheirDataPathsAndNotTheStylePath) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.rankPreferences().range = RangePreference{RecentDays{2}, false};
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 0);
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 1);
}

TEST(BenchmarkTrackingStyle, TheStyleFollowsTheGraphAcrossBenchmarkSelection) {
    StyleFixture fixture;
    fixture.startStyle();
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{4, 4, 4, 255}, std::nullopt};
    fixture.publishStyleEdits();
    const auto committed = fixture.tracking->presentation(GraphKey::BenchmarkRank);

    fixture.tracking->clearSelection();
    fixture.tracking->select(BenchmarkId{"b1"});

    EXPECT_EQ(fixture.tracking->presentation(GraphKey::BenchmarkRank), committed);
    EXPECT_EQ(committed.seriesStyle("rank").color, (RgbaColor{4, 4, 4, 255}));
}

TEST(BenchmarkTrackingStyle, OtherGraphsStyleChangesAreIgnored) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.configuration->configuration.preferences(GraphKey::ScenarioHistory).appearance.connection =
        ConnectionMode::Straight;
    fixture.configuration->configuration.preferences(GraphKey::DailyPlaytime).appearance.connection =
        ConnectionMode::Straight;
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 0);
}
