#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include "profile_importer.h"
#include "run_ingestor.h"
#include "fake_file_service.h"
#include "run_builders.h"

using namespace ksv;

namespace {
    class ProfileImporterTest : public testing::Test {
    protected:
        std::shared_ptr<tests_support::FakeFileService> files = std::make_shared<tests_support::FakeFileService>();
        std::shared_ptr<data::RunIngestor> ingestor = std::make_shared<data::RunIngestor>(files);
        application::ProfileImportRequest request;
        void SetUp() override {
            request.roots = files->source_roots;
            request.baseline = std::make_shared<const domain::UserProfile>();
        }
    };

    TEST_F(ProfileImporterTest, KnownSourcesRequireNoDecodeAndNoCandidate) {
        files->perfs_to_return = {tests_support::makeRun("h", 100)};
        data::ProfileImporter importer(files, ingestor);
        const auto first = importer.run(request, {});
        ASSERT_TRUE(first.changed);
        ASSERT_NE(first.candidate, nullptr);
        EXPECT_EQ(first.summary.added, 1U);
        const auto reads = files->decode_calls;
        request.baseline = first.candidate;
        const auto second = importer.run(request, {});
        EXPECT_FALSE(second.changed);
        EXPECT_EQ(second.candidate, nullptr);
        EXPECT_EQ(files->decode_calls, reads);
    }

    TEST_F(ProfileImporterTest, MissingOldSourcesRetainHistoryAndOlderCopiedRunsAreAdded) {
        auto old = std::make_shared<domain::UserProfile>();
        old->addRun(tests_support::makeRun("old", 500));
        request.baseline = old;
        files->perfs_to_return = {tests_support::makeRun("new", 100)};
        const auto result = data::ProfileImporter(files, ingestor).run(request, {});
        ASSERT_TRUE(result.candidate);
        EXPECT_EQ(result.candidate->getAllRunRecords().size(), 2U);
        EXPECT_EQ(result.candidate->getLatestRun()->run_id.start_time, 500);
    }

    TEST_F(ProfileImporterTest, LateCsvDoesNotDecodePerfAgainAndSuppliesCanonicalTotals) {
        files->perfs_to_return = {tests_support::makeRun("h", 100, 10)};
        data::ProfileImporter importer(files, ingestor);
        const auto first = importer.run(request, {});
        request.baseline = first.candidate;
        files->stats_files = {{files->source_roots.front(), "FPSAimTrainer/stats", "listed-perf-0 Stats.csv"}};
        files->stats_by_path["listed-perf-0 Stats.csv"] = data::ParsedStatsCsv{
            .scenario_id = {.name = "Scenario h", .hash = "h"}, .totals = {.score = 99}};
        const auto result = importer.run(request, {});
        ASSERT_TRUE(result.candidate);
        EXPECT_EQ(result.summary.enriched, 1U);
        EXPECT_EQ(result.candidate->getAllRunRecords().size(), 1U);
        EXPECT_FLOAT_EQ(result.candidate->getLatestRun()->stored_totals.score, 99);
        EXPECT_EQ(files->decode_calls, 1U);
    }

    TEST_F(ProfileImporterTest, FailedSourceRemainsEligibleAfterItBecomesReadable) {
        files->perfs_to_return = {tests_support::makeRun("h", 100)};
        files->throw_for_indices.insert(0);
        data::ProfileImporter importer(files, ingestor);
        const auto failed = importer.run(request, {});
        EXPECT_FALSE(failed.changed);
        ASSERT_EQ(failed.summary.issues.size(), 1U);
        files->throw_for_indices.clear();
        const auto retried = importer.run(request, {});
        EXPECT_TRUE(retried.changed);
        EXPECT_EQ(retried.summary.added, 1U);
    }

    TEST_F(ProfileImporterTest, DuplicateAcrossRootsIsRememberedForTheSession) {
        files->explicit_perfs = std::vector<application::PerfFile>{
            {"C:/A", "FPSAimTrainer/performances", "same.perf"},
            {"C:/B", "FPSAimTrainer/performances", "same.perf"}};
        files->perfs_by_path["same.perf"] = tests_support::makeRun("h", 100);
        data::ProfileImporter importer(files, ingestor);
        const auto first = importer.run(request, {});
        ASSERT_TRUE(first.candidate);
        EXPECT_EQ(first.candidate->getAllRunRecords().size(), 1U);
        EXPECT_EQ(first.summary.added, 1U);
        request.baseline = first.candidate;
        request.known_aliases = first.successful_aliases;
        const auto decodes = files->decode_calls;
        EXPECT_FALSE(importer.run(request, {}).changed);
        EXPECT_EQ(files->decode_calls, decodes);
    }

    TEST_F(ProfileImporterTest, DifferentPerfSamplesAtSameIdAndDurationDoNotReplacePrimary) {
        auto original = tests_support::makeRun("h", 100, 10, 5, 3, 30);
        original.performance.emplace();
        original.performance->add_data(1.0F, domain::HITS, 3);
        auto baseline = std::make_shared<domain::UserProfile>();
        original.sources.perf = {baseline->ensureSource("C:/A", "FPSAimTrainer/performances"), "old.perf"};
        baseline->addRun(original);
        request.baseline = baseline;
        files->explicit_perfs = std::vector<application::PerfFile>{{"C:/B", "FPSAimTrainer/performances", "copy.perf"}};
        auto other = tests_support::makeRun("h", 100, 10, 5, 3, 30);
        other.performance.emplace();
        other.performance->add_data(1.0F, domain::HITS, 4);
        files->perfs_by_path["copy.perf"] = other;
        const auto result = data::ProfileImporter(files, ingestor).run(request, {});
        EXPECT_FALSE(result.changed);
        ASSERT_EQ(result.summary.issues.size(), 1U);
        EXPECT_TRUE(result.successful_aliases.empty());
    }

    TEST_F(ProfileImporterTest, CsvFirstThenPerfConvergesToOneCanonicalRun) {
        const std::string stem = "Run - Challenge - 2026.08.27-02.26.40";
        files->stats_files = {{files->source_roots.front(), "FPSAimTrainer/stats", stem + " Stats.csv"}};
        files->stats_by_path[stem + " Stats.csv"] = data::ParsedStatsCsv{
            .scenario_id = {.name = "Scenario h", .hash = "h"},
            .totals = {.score = 99}, .challenge_start = std::chrono::hours{2} + std::chrono::minutes{25}};
        data::ProfileImporter importer(files, ingestor);
        const auto first = importer.run(request, {});
        ASSERT_TRUE(first.candidate);
        const auto old_id = first.candidate->getLatestRun()->run_id;
        request.baseline = first.candidate;
        files->explicit_perfs = std::vector<application::PerfFile>{{files->source_roots.front(),
            "FPSAimTrainer/performances", stem + " Performance.perf"}};
        auto perf = tests_support::makeRun("h", 123, 10, 5, 3, 30);
        perf.performance.emplace();
        files->perfs_by_path[stem + " Performance.perf"] = perf;
        const auto result = importer.run(request, {});
        ASSERT_TRUE(result.candidate);
        EXPECT_EQ(result.summary.enriched, 1U);
        EXPECT_EQ(result.candidate->getAllRunRecords().size(), 1U);
        EXPECT_FALSE(result.candidate->getCurrentRun(old_id));
        EXPECT_EQ(result.candidate->getLatestRun()->run_id.start_time, 123);
        EXPECT_FLOAT_EQ(result.candidate->getLatestRun()->stored_totals.score, 99);
        EXPECT_TRUE(result.candidate->getLatestRun()->performance);
        EXPECT_EQ(files->parse_calls, 1U);
    }

    TEST_F(ProfileImporterTest, PerfWithIncompleteMetadataIsNotRecordedAsImported) {
        files->perfs_to_return = {domain::Run{}};
        const auto failed = data::ProfileImporter(files, ingestor).run(request, {});
        EXPECT_FALSE(failed.changed);
        EXPECT_TRUE(failed.successful_aliases.empty());
        ASSERT_EQ(failed.summary.issues.size(), 1U);
        EXPECT_TRUE(failed.summary.issues[0].retryable);
    }

    // listPerfFiles() names its perfs "listed-perf-<i>"; a stats file sharing that stem pairs with it.
    application::StatsFile statsFile(const std::string &filename) {
        return {.root = "fake/kovaaks", .subdir = "FPSAimTrainer/stats", .filename = filename};
    }

    data::ParsedStatsCsv csvTotals(const std::string &hash, const float score) {
        return {.scenario_id = {.name = "Scenario " + hash, .hash = hash},
                .totals = {.score = score},
                .stats = {.dpi = 800}};
    }

    data::ParsedStatsCsv csvTimed(const std::string &hash) {
        using namespace std::chrono;
        return {.scenario_id = {.name = "Scenario " + hash, .hash = hash},
                .totals = {.score = 1.0F, .shots = 10, .hits = 8, .misses = 2},
                .challenge_start = hours{2} + minutes{25} + seconds{40} + milliseconds{148}};
    }

    constexpr auto kSoloCsv = "Solo - Challenge - 2026.08.27-02.26.40 Stats.csv";

    TEST_F(ProfileImporterTest, AllFailedScanReturnsNoCandidateAndKeepsTheBaseline) {
        auto baseline = std::make_shared<domain::UserProfile>();
        baseline->addRun(tests_support::makeRun("old", 500));
        request.baseline = baseline;
        files->scan_issues = {{"fake/kovaaks", "Source directory is unavailable or unreadable", true}};
        const auto result = data::ProfileImporter(files, ingestor).run(request, {});
        EXPECT_FALSE(result.changed);
        EXPECT_FALSE(result.failed);
        EXPECT_EQ(result.candidate, nullptr);
        ASSERT_EQ(result.summary.issues.size(), 1U);
        EXPECT_TRUE(result.summary.issues[0].retryable);
        EXPECT_EQ(baseline->getAllRunRecords().size(), 1U);
    }

    TEST_F(ProfileImporterTest, ProgressTotalsEqualCandidateGroupsAndAPairedPerfAndCsvCountsOnce) {
        files->perfs_to_return = {tests_support::makeRun("hash-1", 100, 1.0F), tests_support::makeRun("hash-2", 200)};
        files->stats_files = {statsFile("listed-perf-0 Stats.csv"), statsFile(kSoloCsv)};
        files->stats_by_path.emplace("listed-perf-0 Stats.csv", csvTotals("hash-1", 1.0F));
        files->stats_by_path.emplace(kSoloCsv, csvTimed("solo"));

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        const auto result = data::ProfileImporter(files, ingestor).run(request,
            [&reports](const application::ProfileImportPhase phase, const std::size_t done, const std::size_t total) {
                if (phase == application::ProfileImportPhase::Importing) reports.emplace_back(done, total);
            });

        // paired (perf 0 + sibling), perf-only (perf 1), csv-only (Solo)
        ASSERT_EQ(reports.size(), 3U);
        for (std::size_t i = 0; i < reports.size(); ++i) EXPECT_EQ(reports[i], (std::pair<std::size_t, std::size_t>{i + 1, 3}));
        EXPECT_EQ(result.candidate->getAllRunRecords().size(), 3U);
    }

    TEST_F(ProfileImporterTest, PerfWithItsStatsSiblingBecomesOneRunCarryingBothSources) {
        auto perf = tests_support::makeRun("hash-1", 100, 42.0F);
        perf.performance.emplace();
        files->perfs_to_return = {perf};
        files->stats_files = {statsFile("listed-perf-0 Stats.csv")};
        files->stats_by_path.emplace("listed-perf-0 Stats.csv", csvTotals("hash-1", 42.0F));

        const auto result = data::ProfileImporter(files, ingestor).run(request, {});

        ASSERT_TRUE(result.candidate);
        ASSERT_EQ(result.candidate->getAllRunRecords().size(), 1U);
        const auto &run = result.candidate->getAllRunRecords().front();
        EXPECT_TRUE(run.performance.has_value());
        ASSERT_TRUE(run.stats.has_value());
        EXPECT_EQ(run.stats->dpi, 800);
        EXPECT_TRUE(run.sources.perf.has_value());
        EXPECT_TRUE(run.sources.csv.has_value());
        const auto &entries = result.candidate->sources().entries();
        EXPECT_TRUE(std::ranges::any_of(entries, [](const auto &e) { return e.path == "FPSAimTrainer/performances"; }));
        EXPECT_TRUE(std::ranges::any_of(entries, [](const auto &e) { return e.path == "FPSAimTrainer/stats"; }));
    }

    TEST_F(ProfileImporterTest, CsvOnlyGroupIsIngestedWithDerivedTiming) {
        files->stats_files = {statsFile(kSoloCsv)};
        files->stats_by_path.emplace(kSoloCsv, csvTimed("solo"));

        const auto result = data::ProfileImporter(files, ingestor).run(request, {});

        ASSERT_TRUE(result.candidate);
        ASSERT_EQ(result.candidate->getAllRunRecords().size(), 1U);
        const auto &run = result.candidate->getAllRunRecords().front();
        EXPECT_EQ(run.run_id.scenario_id.hash, "solo");
        EXPECT_FALSE(run.performance.has_value());
        EXPECT_NEAR(run.scenario_length, 59.852F, 0.01F);
        EXPECT_TRUE(run.sources.csv.has_value());
        EXPECT_FALSE(run.sources.perf.has_value());
    }

    TEST_F(ProfileImporterTest, LatePerfRekeysACsvOnlyRunAndPointsItsAliasesAtTheNewId) {
        const std::string stem = "Run - Challenge - 2026.08.27-02.26.40";
        files->stats_files = {{files->source_roots.front(), "FPSAimTrainer/stats", stem + " Stats.csv"}};
        files->stats_by_path[stem + " Stats.csv"] = data::ParsedStatsCsv{
            .scenario_id = {.name = "Scenario h", .hash = "h"},
            .totals = {.score = 99}, .challenge_start = std::chrono::hours{2} + std::chrono::minutes{25}};
        data::ProfileImporter importer(files, ingestor);
        const auto first = importer.run(request, {});
        ASSERT_TRUE(first.candidate);
        const auto old_id = first.candidate->getLatestRun()->run_id;
        ASSERT_EQ(first.successful_aliases.size(), 1U);
        EXPECT_EQ(first.successful_aliases[0].run_id, old_id);

        request.baseline = first.candidate;
        request.known_aliases = first.successful_aliases;
        files->explicit_perfs = std::vector<application::PerfFile>{{files->source_roots.front(),
            "FPSAimTrainer/performances", stem + " Performance.perf"}};
        auto perf = tests_support::makeRun("h", 123, 10, 5, 3, 30);
        perf.performance.emplace();
        files->perfs_by_path[stem + " Performance.perf"] = perf;
        const auto second = importer.run(request, {});

        ASSERT_EQ(second.rekeyed_runs.size(), 1U);
        EXPECT_EQ(second.rekeyed_runs[0].first, old_id);
        const auto new_id = second.rekeyed_runs[0].second;
        EXPECT_EQ(new_id, second.candidate->getLatestRun()->run_id);
        ASSERT_FALSE(second.successful_aliases.empty());
        for (const auto &alias : second.successful_aliases) EXPECT_EQ(alias.run_id, new_id);
    }

    TEST_F(ProfileImporterTest, RebuildWithZeroSourcesReturnsAChangedEmptyCandidate) {
        auto baseline = std::make_shared<domain::UserProfile>();
        baseline->addRun(tests_support::makeRun("old", 500));
        request.baseline = baseline;
        request.mode = application::ProfileImportMode::Rebuild;
        const auto result = data::ProfileImporter(files, ingestor).run(request, {});
        EXPECT_TRUE(result.changed);
        ASSERT_TRUE(result.candidate);
        EXPECT_TRUE(result.candidate->getAllRunRecords().empty());
    }

    class FailedSourceImportTest : public ProfileImporterTest {
    protected:
        const std::string path = "fake/kovaaks/FPSAimTrainer/performances/listed-perf-0";
        data::ProfileImporter importer{files, ingestor};

        void SetUp() override {
            ProfileImporterTest::SetUp();
            files->perfs_to_return = {tests_support::makeRun("h", 100)};
            files->throw_for_indices.insert(0);
            files->fingerprints[path] = {10, 20};
        }

        std::shared_ptr<const domain::UserProfile> recordFailure() {
            request.reason = application::ProfileImportReason::Manual;
            const auto result = importer.run(request, {});
            EXPECT_EQ(result.failed_sources.size(), 1U);
            return result.candidate;
        }

        void automatic(application::ProfileImportReason reason) {
            request.reason = reason;
            request.skip_unchanged_failures = true;
        }
    };

    TEST_F(FailedSourceImportTest, FailingPerfOnManualRefreshIsRecordedWithItsFingerprint) {
        const auto result = importer.run(request, {});
        EXPECT_FALSE(result.changed);
        EXPECT_TRUE(result.failures_changed);
        ASSERT_TRUE(result.candidate);
        EXPECT_EQ(result.summary.added, 0U);
        ASSERT_EQ(result.failed_sources.size(), 1U);
        const auto &failed = result.failed_sources.front();
        EXPECT_EQ(failed.size, 10);
        EXPECT_EQ(failed.modified_ms, 20);
        EXPECT_EQ(failed.message, "File does not exist");
        EXPECT_EQ(result.candidate->sources().resolve(failed.file), path);
        EXPECT_EQ(result.candidate->failedSources(), result.failed_sources);
    }

    TEST_F(FailedSourceImportTest, StartupSkipsAnUnchangedRecordedFailureWithoutDecoding) {
        request.baseline = recordFailure();
        automatic(application::ProfileImportReason::Startup);
        const auto decodes = files->decode_calls;
        const auto result = importer.run(request, {});
        EXPECT_EQ(files->decode_calls, decodes);
        ASSERT_EQ(result.summary.issues.size(), 1U);
        EXPECT_EQ(result.summary.issues.front().path, path);
        EXPECT_EQ(result.summary.issues.front().message, "File does not exist");
        EXPECT_TRUE(result.summary.issues.front().previously_failed);
        EXPECT_FALSE(result.summary.issues.front().retryable);
        EXPECT_FALSE(result.failures_changed);
        EXPECT_FALSE(result.changed);
        EXPECT_EQ(result.candidate, nullptr);
        EXPECT_EQ(result.failed_sources, request.baseline->failedSources());
    }

    TEST_F(FailedSourceImportTest, LiveRetriesAChangedFingerprintAndDropsTheRecordOnSuccess) {
        request.baseline = recordFailure();
        files->throw_for_indices.clear();
        files->fingerprints[path] = {11, 20};
        automatic(application::ProfileImportReason::Live);
        const auto decodes = files->decode_calls;
        const auto result = importer.run(request, {});
        EXPECT_EQ(files->decode_calls, decodes + 1);
        EXPECT_EQ(result.summary.added, 1U);
        EXPECT_TRUE(result.failures_changed);
        ASSERT_TRUE(result.candidate);
        EXPECT_TRUE(result.candidate->failedSources().empty());
        EXPECT_TRUE(result.failed_sources.empty());
    }

    TEST_F(FailedSourceImportTest, ManualAndRetryDecodeARecordedFailureWithAnUnchangedFingerprint) {
        request.baseline = recordFailure();
        for (const auto reason : {application::ProfileImportReason::Manual, application::ProfileImportReason::Retry}) {
            request.reason = reason;
            request.skip_unchanged_failures = false;
            const auto decodes = files->decode_calls;
            const auto result = importer.run(request, {});
            EXPECT_EQ(files->decode_calls, decodes + 1);
            ASSERT_EQ(result.summary.issues.size(), 1U);
            EXPECT_FALSE(result.summary.issues.front().previously_failed);
            EXPECT_FALSE(result.failures_changed);
        }
    }

    TEST_F(FailedSourceImportTest, RebuildIgnoresBaselineRecordsAndRecordsCurrentFailures) {
        request.baseline = recordFailure();
        request.mode = application::ProfileImportMode::Rebuild;
        request.skip_unchanged_failures = true;
        const auto decodes = files->decode_calls;
        const auto result = importer.run(request, {});
        EXPECT_EQ(files->decode_calls, decodes + 1);
        ASSERT_EQ(result.summary.issues.size(), 1U);
        EXPECT_FALSE(result.summary.issues.front().previously_failed);
        ASSERT_TRUE(result.candidate);
        ASSERT_EQ(result.candidate->failedSources().size(), 1U);
        EXPECT_EQ(result.candidate->sources().resolve(result.candidate->failedSources().front().file), path);
    }

    TEST_F(FailedSourceImportTest, RecordedFileAbsentFromTheScanIsPruned) {
        request.baseline = recordFailure();
        files->perfs_to_return.clear();
        automatic(application::ProfileImportReason::Startup);
        const auto result = importer.run(request, {});
        EXPECT_TRUE(result.failures_changed);
        EXPECT_FALSE(result.changed);
        ASSERT_TRUE(result.candidate);
        EXPECT_TRUE(result.candidate->failedSources().empty());
    }

    TEST_F(ProfileImporterTest, ConflictingPairRecordsBothItsPerfAndItsCsv) {
        auto baseline = std::make_shared<domain::UserProfile>();
        auto existing = tests_support::makeRun("hash-1", 100, 42.0F, 0, 0, 30.0F);
        existing.sources.perf = {baseline->ensureSource(files->source_roots.front(), "FPSAimTrainer/performances"),
            "other.perf"};
        baseline->addRun(existing);
        request.baseline = baseline;
        auto perf = tests_support::makeRun("hash-1", 100, 42.0F);
        perf.performance.emplace();
        files->perfs_to_return = {perf};
        files->stats_files = {statsFile("listed-perf-0 Stats.csv")};
        files->stats_by_path.emplace("listed-perf-0 Stats.csv", csvTotals("hash-1", 42.0F));

        const auto result = data::ProfileImporter(files, ingestor).run(request, {});

        ASSERT_EQ(result.summary.issues.size(), 2U);
        ASSERT_EQ(result.failed_sources.size(), 2U);
        ASSERT_TRUE(result.candidate);
        std::vector<std::string> recorded;
        for (const auto &failed : result.failed_sources)
            recorded.push_back(result.candidate->sources().resolve(failed.file).value_or(""));
        std::ranges::sort(recorded);
        EXPECT_EQ(recorded, (std::vector<std::string>{"fake/kovaaks/FPSAimTrainer/performances/listed-perf-0",
            "fake/kovaaks/FPSAimTrainer/stats/listed-perf-0 Stats.csv"}));
    }

    class CsvDamageImportTest : public ProfileImporterTest {
    protected:
        application::ProfileImportResult importPairAgainstBaselineDamage(const std::optional<float> baseline_damage,
                                                                         const std::optional<float> incoming_damage) {
            auto baseline = std::make_shared<domain::UserProfile>();
            auto existing = tests_support::makeRun("hash-1", 100, 42.0F, 0, 0, 30.0F);
            existing.stored_totals.damage = baseline_damage;
            existing.stats = csvTotals("hash-1", 42.0F).stats;
            existing.sources.csv = {baseline->ensureSource(files->source_roots.front(), "FPSAimTrainer/stats"),
                "other Stats.csv"};
            baseline->addRun(existing);
            request.baseline = baseline;
            auto perf = tests_support::makeRun("hash-1", 100, 42.0F);
            perf.performance.emplace();
            files->perfs_to_return = {perf};
            files->stats_files = {statsFile("listed-perf-0 Stats.csv")};
            auto csv = csvTotals("hash-1", 42.0F);
            csv.totals.damage = incoming_damage;
            files->stats_by_path.emplace("listed-perf-0 Stats.csv", csv);
            return data::ProfileImporter(files, ingestor).run(request, {});
        }
    };

    TEST_F(CsvDamageImportTest, PreUpgradeRunWithoutDamageIsNotConflictedByTheSameCsvCarryingDamage) {
        const auto result = importPairAgainstBaselineDamage(std::nullopt, 166.0F);
        EXPECT_TRUE(result.summary.issues.empty());
        EXPECT_TRUE(result.failed_sources.empty());
    }

    TEST_F(CsvDamageImportTest, DifferentPresentDamageValuesStillConflict) {
        const auto result = importPairAgainstBaselineDamage(100.0F, 166.0F);
        EXPECT_FALSE(result.summary.issues.empty());
        EXPECT_FALSE(result.failed_sources.empty());
    }

    TEST_F(FailedSourceImportTest, DirectoryLevelScanIssueIsNeverRecorded) {
        files->perfs_to_return.clear();
        files->scan_issues = {{"fake/kovaaks", "Source directory is unavailable or unreadable", true}};
        const auto result = importer.run(request, {});
        EXPECT_EQ(result.summary.issues.size(), 1U);
        EXPECT_FALSE(result.failures_changed);
        EXPECT_TRUE(result.failed_sources.empty());
    }
}
