#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "data/run_ingestor.h"
#include "fake_file_service.h"

namespace {
    ksv::application::PerfFile perf(const std::string_view filename, const std::string_view root = "root") {
        return {.root = std::string(root), .subdir = "FPSAimTrainer/performances", .filename = std::string(filename)};
    }

    ksv::application::StatsFile csv(const std::string_view filename, const std::string_view root = "root") {
        return {.root = std::string(root), .subdir = "FPSAimTrainer/stats", .filename = std::string(filename)};
    }

    // RAII redirect of std::cerr so diagnostic-only logging can be asserted on.
    class CerrCapture {
    public:
        CerrCapture() : m_previous(std::cerr.rdbuf(m_sink.rdbuf())) {}
        ~CerrCapture() { std::cerr.rdbuf(m_previous); }
        [[nodiscard]] std::string str() const { return m_sink.str(); }

    private:
        std::ostringstream m_sink;
        std::streambuf *m_previous;
    };

    ksv::data::ParsedStatsCsv fixtureCsv() {
        using namespace std::chrono;
        return {.scenario_id = {.name = "1wall6targets TE", .hash = "hash"},
                .totals = {.score = 153.088882F, .shots = 180, .hits = 166, .misses = 14, .kills = 166},
                .stats = {.sens_scale = "Valorant", .dpi = 1600},
                .challenge_start = hours{2} + minutes{25} + seconds{40} + milliseconds{148}};
    }

    constexpr std::string_view kTimestampedCsv = "1wall6targets TE - Challenge - 2026.08.27-02.26.40 Stats.csv";

    TEST(PairRunFilesTest, GroupsPairsAndBothKindsOfLeftover) {
        const auto groups = ksv::data::pairRunFiles(
            {perf("A Performance.perf"), perf("B Performance.perf")},
            {csv("A Stats.csv"), csv("C Stats.csv")});

        ASSERT_EQ(groups.size(), 3U);
        EXPECT_TRUE(groups[0].perf && groups[0].csv);
        EXPECT_TRUE(groups[1].perf && !groups[1].csv);
        EXPECT_TRUE(!groups[2].perf && groups[2].csv);
    }

    TEST(PairRunFilesTest, OrdersGroupsByRootThenStem) {
        const auto groups = ksv::data::pairRunFiles(
            {perf("B Performance.perf", "z"), perf("A Performance.perf", "z")},
            {csv("C Stats.csv", "a")});

        ASSERT_EQ(groups.size(), 3U);
        EXPECT_EQ(groups[0].csv->root, "a");
        EXPECT_EQ(groups[1].perf->filename, "A Performance.perf");
        EXPECT_EQ(groups[2].perf->filename, "B Performance.perf");
    }

    TEST(RunIngestorTest, PairedRunUsesCsvTotalsAndKeepsPerfFacet) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        ksv::domain::Run perf_run{.run_id = {.scenario_id = {.name = "Perf", .hash = "hash"}, .start_time = 7}, .scenario_length = 60.0F};
        perf_run.performance.emplace();
        files->perfs_by_path.emplace("A Performance.perf", perf_run);
        files->stats_by_path.emplace("A Stats.csv", ksv::data::ParsedStatsCsv{
            .scenario_id = {.name = "Csv", .hash = "hash"}, .totals = {.score = 99.0F, .hits = 8, .misses = 2}, .stats = {.dpi = 800},
            .pause_duration = std::chrono::milliseconds{5000}});
        ksv::domain::UserProfile profile;

        const auto run = ksv::data::RunIngestor(files).buildRun(profile, perf("A Performance.perf"), csv("A Stats.csv"));

        ASSERT_TRUE(run);
        EXPECT_EQ(run->run_id.start_time, 7);
        EXPECT_FLOAT_EQ(run->scenario_length, 60.0F);
        EXPECT_TRUE(run->performance);
        EXPECT_EQ(run->totals().score, 99.0F);
        ASSERT_TRUE(run->stats);
        EXPECT_EQ(run->stats->dpi, 800);
    }

    TEST(RunIngestorTest, CsvOnlyRunDerivesKeyAndLengthAndHasNoPerformance) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        files->stats_by_path.emplace(std::string(kTimestampedCsv), fixtureCsv());
        ksv::domain::UserProfile profile;

        const auto run = ksv::data::RunIngestor(files).buildRun(profile, std::nullopt, csv(kTimestampedCsv));

        ASSERT_TRUE(run);
        EXPECT_EQ(run->run_id.scenario_id.hash, "hash");
        EXPECT_NEAR(run->scenario_length, 59.852F, 0.001F);
        EXPECT_FALSE(run->performance.has_value());
        EXPECT_FALSE(run->sources.perf.has_value());
        ASSERT_TRUE(run->sources.csv.has_value());
        EXPECT_EQ(run->totals().shots, 180);
    }

    TEST(RunIngestorTest, PerfOnlyRunKeepsDecoderTotalsAndHasNoStats) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        ksv::domain::Run perf_run{.run_id = {.scenario_id = {.name = "Perf", .hash = "hash"}, .start_time = 11},
                                  .scenario_length = 42.0F};
        perf_run.performance.emplace();
        perf_run.stored_totals = {.score = 7.5F, .shots = 4, .hits = 3, .misses = 1};
        files->perfs_by_path.emplace("A Performance.perf", perf_run);
        ksv::domain::UserProfile profile;

        const auto run = ksv::data::RunIngestor(files).buildRun(profile, perf("A Performance.perf"), std::nullopt);

        ASSERT_TRUE(run);
        EXPECT_EQ(run->totals().score, 7.5F);
        EXPECT_EQ(run->totals().hits, 3);
        EXPECT_FALSE(run->stats.has_value());
        EXPECT_FALSE(run->sources.csv.has_value());
    }

    TEST(RunIngestorTest, PairedHashOrCountMismatchIsLoggedButRunIsKept) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        ksv::domain::Run perf_run{.run_id = {.scenario_id = {.name = "Perf", .hash = "perf-hash"}, .start_time = 7},
                                  .scenario_length = 60.0F};
        perf_run.performance.emplace();
        perf_run.stored_totals = {.score = 10.0F, .hits = 3, .misses = 1};
        files->perfs_by_path.emplace("A Performance.perf", perf_run);
        files->stats_by_path.emplace("A Stats.csv", ksv::data::ParsedStatsCsv{
            .scenario_id = {.name = "Csv", .hash = "csv-hash"}, .totals = {.score = 99.0F, .hits = 8, .misses = 2}});
        ksv::domain::UserProfile profile;

        std::optional<ksv::domain::Run> run;
        std::string logged;
        {
            const CerrCapture capture;
            run = ksv::data::RunIngestor(files).buildRun(profile, perf("A Performance.perf"), csv("A Stats.csv"));
            logged = capture.str();
        }

        ASSERT_TRUE(run);
        EXPECT_EQ(run->totals().score, 99.0F);
        EXPECT_TRUE(run->performance.has_value());
        EXPECT_NE(logged.find("differ"), std::string::npos);
    }

    TEST(RunIngestorTest, PairedScoreWithinToleranceIsNotFlagged) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        ksv::domain::Run perf_run{.run_id = {.scenario_id = {.name = "S", .hash = "hash"}, .start_time = 7},
                                  .scenario_length = 60.0F};
        perf_run.performance.emplace();
        perf_run.stored_totals = {.score = 153.08887F, .hits = 166, .misses = 14};
        files->perfs_by_path.emplace("A Performance.perf", perf_run);
        files->stats_by_path.emplace("A Stats.csv", ksv::data::ParsedStatsCsv{
            .scenario_id = {.name = "S", .hash = "hash"},
            .totals = {.score = 153.088882F, .hits = 166, .misses = 14}});
        ksv::domain::UserProfile profile;

        std::string logged;
        {
            const CerrCapture capture;
            std::ignore = ksv::data::RunIngestor(files).buildRun(profile, perf("A Performance.perf"), csv("A Stats.csv"));
            logged = capture.str();
        }

        EXPECT_EQ(logged.find("differ"), std::string::npos);
    }

    TEST(RunIngestorTest, CsvOnlyRunSubtractsPauseDurationFromLength) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        auto parsed = fixtureCsv();
        parsed.pause_count = 2;
        parsed.pause_duration = std::chrono::milliseconds{5000};
        files->stats_by_path.emplace(std::string(kTimestampedCsv), parsed);
        ksv::domain::UserProfile profile;

        const auto run = ksv::data::RunIngestor(files).buildRun(profile, std::nullopt, csv(kTimestampedCsv));

        ASSERT_TRUE(run);
        EXPECT_NEAR(run->scenario_length, 54.852F, 0.001F);
    }

    TEST(RunIngestorTest, CsvOnlyRunClampsLengthToZeroWhenPauseExceedsSpan) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        auto parsed = fixtureCsv();
        parsed.pause_duration = std::chrono::milliseconds{120000};
        files->stats_by_path.emplace(std::string(kTimestampedCsv), parsed);
        ksv::domain::UserProfile profile;

        const auto run = ksv::data::RunIngestor(files).buildRun(profile, std::nullopt, csv(kTimestampedCsv));

        ASSERT_TRUE(run);
        EXPECT_FLOAT_EQ(run->scenario_length, 0.0F);
    }

    TEST(RunIngestorTest, CorruptPerfInAPairedGroupStillYieldsACsvOnlyRun) {
        auto files = std::make_shared<ksv::tests_support::FakeFileService>();
        files->paths_to_throw_for.insert(perf("Broken Performance.perf").absolutePath());
        auto parsed = fixtureCsv();
        files->stats_by_path.emplace(std::string(kTimestampedCsv), parsed);
        ksv::domain::UserProfile profile;

        std::optional<ksv::domain::Run> run;
        {
            const CerrCapture capture;
            run = ksv::data::RunIngestor(files).buildRun(
                profile, perf("Broken Performance.perf"), csv(kTimestampedCsv));
        }

        ASSERT_TRUE(run);
        EXPECT_FALSE(run->performance.has_value());
        EXPECT_FALSE(run->sources.perf.has_value());
        ASSERT_TRUE(run->sources.csv.has_value());
        EXPECT_EQ(run->totals().shots, 180);
    }
}
