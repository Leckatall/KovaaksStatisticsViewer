//
// ProfileBuilder tests using a hand-written fake IFileService.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "profile_builder.h"
#include "run_ingestor.h"
#include "fake_file_service.h"

using namespace ksv::data;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    ksv::domain::Run make_perf(const std::string &hash, const long long start_time,
                                        const float score = 0.0F) {
        ksv::domain::Run perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.stored_totals.score = score;
        return perf;
    }

    class ProfileBuilderTest : public testing::Test {
    protected:
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        std::shared_ptr<IRunIngestor> ingestor = std::make_shared<RunIngestor>(fake_file_service);
        ProfileBuilder builder{fake_file_service, ingestor};
    };

    TEST_F(ProfileBuilderTest, BuildRegistersEveryConfiguredRoot) {
        fake_file_service->source_roots = {"C:/Kovaaks", "D:/Kovaaks"};

        const auto profile = builder.build();
        const auto &entries = profile.sources().entries();
        ASSERT_EQ(entries.size(), 4);
        EXPECT_EQ(entries[0].path, "C:/Kovaaks");
        EXPECT_EQ(entries[2].path, "D:/Kovaaks");
    }

    TEST_F(ProfileBuilderTest, BuildAggregatesEveryPerfFromTheDirectory) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-1", 200), make_perf("hash-2", 300)
        };

        const auto profile = builder.build();

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 2);
        EXPECT_EQ(profile.getRunCount(scenarios[0]).value_or(0) +
                  profile.getRunCount(scenarios[1]).value_or(0), 3);
        for (const auto &run : profile.getAllRunRecords()) {
            ASSERT_TRUE(run.sources.perf.has_value());
            EXPECT_TRUE(profile.sources().resolve(*run.sources.perf).has_value());
        }
    }

    TEST_F(ProfileBuilderTest, BuildReportsProgressOncePerFile) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 200), make_perf("hash-3", 300)
        };

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        std::ignore = builder.build([&reports](const std::size_t done, const std::size_t total) {
            reports.emplace_back(done, total);
        });

        ASSERT_EQ(reports.size(), 3);
        EXPECT_EQ(reports[0], (std::pair<std::size_t, std::size_t>{1, 3}));
        EXPECT_EQ(reports[1], (std::pair<std::size_t, std::size_t>{2, 3}));
        EXPECT_EQ(reports[2], (std::pair<std::size_t, std::size_t>{3, 3}));
    }

    TEST_F(ProfileBuilderTest, BuildSkipsAFileThatVanishedDuringDecodeButAggregatesTheRest) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 200), make_perf("hash-3", 300)
        };
        fake_file_service->throw_for_indices.insert(1);

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        const auto profile = builder.build([&reports](const std::size_t done, const std::size_t total) {
            reports.emplace_back(done, total);
        });

        EXPECT_EQ(profile.getAllRunRecords().size(), 2);
        ASSERT_FALSE(reports.empty());
        EXPECT_EQ(reports.back(), (std::pair<std::size_t, std::size_t>{3, 3}));
    }

    TEST_F(ProfileBuilderTest, BuildReportsNoProgressForAnEmptyDirectory) {
        int report_count = 0;
        std::ignore = builder.build([&report_count](std::size_t, std::size_t) { ++report_count; });

        EXPECT_EQ(report_count, 0);
    }

    TEST_F(ProfileBuilderTest, BuildIsRepeatable) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        const auto first = builder.build();
        const auto second = builder.build();

        EXPECT_EQ(first.getScenarioList().size(), 1);

        EXPECT_EQ(second.getScenarioList().size(), 1);
    }

    // listPerfFiles() synthesises "listed-perf-<i>" names for each perfs_to_return entry;
    // a stats file sharing that stem pairs with it.
    ksv::application::StatsFile stats_file(const std::string &filename) {
        return {.root = "fake/kovaaks", .subdir = "FPSAimTrainer/stats", .filename = filename};
    }

    ksv::data::ParsedStatsCsv csv_totals(const std::string &hash, const float score) {
        return {.scenario_id = {.name = "Scenario " + hash, .hash = hash},
                .totals = {.score = score},
                .stats = {.dpi = 800}};
    }

    ksv::data::ParsedStatsCsv csv_timed(const std::string &hash) {
        using namespace std::chrono;
        return {.scenario_id = {.name = "Scenario " + hash, .hash = hash},
                .totals = {.score = 1.0F, .shots = 10, .hits = 8, .misses = 2},
                .challenge_start = hours{2} + minutes{25} + seconds{40} + milliseconds{148}};
    }

    TEST_F(ProfileBuilderTest, BuildPairsPerfWithItsStatsSiblingIntoOneRun) {
        auto paired_perf = make_perf("hash-1", 100, 42.0F);
        paired_perf.performance.emplace();
        fake_file_service->perfs_to_return = {paired_perf};
        fake_file_service->stats_files = {stats_file("listed-perf-0 Stats.csv")};
        fake_file_service->stats_by_path.emplace("listed-perf-0 Stats.csv", csv_totals("hash-1", 42.0F));

        const auto profile = builder.build();

        ASSERT_EQ(profile.getAllRunRecords().size(), 1U);
        const auto &run = profile.getAllRunRecords().front();
        EXPECT_TRUE(run.performance.has_value());
        ASSERT_TRUE(run.stats.has_value());
        EXPECT_EQ(run.stats->dpi, 800);
        EXPECT_TRUE(run.sources.perf.has_value());
        EXPECT_TRUE(run.sources.csv.has_value());
    }

    TEST_F(ProfileBuilderTest, BuildIngestsACsvOnlyGroupWithDerivedTiming) {
        fake_file_service->stats_files = {
            stats_file("Solo - Challenge - 2026.08.27-02.26.40 Stats.csv")};
        fake_file_service->stats_by_path.emplace(
            "Solo - Challenge - 2026.08.27-02.26.40 Stats.csv", csv_timed("solo"));

        const auto profile = builder.build();

        ASSERT_EQ(profile.getAllRunRecords().size(), 1U);
        const auto &run = profile.getAllRunRecords().front();
        EXPECT_EQ(run.run_id.scenario_id.hash, "solo");
        EXPECT_FALSE(run.performance.has_value());
        EXPECT_NEAR(run.scenario_length, 59.852F, 0.01F);
        EXPECT_TRUE(run.sources.csv.has_value());
        EXPECT_FALSE(run.sources.perf.has_value());
    }

    TEST_F(ProfileBuilderTest, BuildReportsProgressOncePerGroupAcrossMixedGroupKinds) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100, 1.0F), make_perf("hash-2", 200)};
        fake_file_service->stats_files = {
            stats_file("listed-perf-0 Stats.csv"),
            stats_file("Solo - Challenge - 2026.08.27-02.26.40 Stats.csv")};
        fake_file_service->stats_by_path.emplace("listed-perf-0 Stats.csv", csv_totals("hash-1", 1.0F));
        fake_file_service->stats_by_path.emplace(
            "Solo - Challenge - 2026.08.27-02.26.40 Stats.csv", csv_timed("solo"));

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        const auto profile = builder.build([&reports](const std::size_t done, const std::size_t total) {
            reports.emplace_back(done, total);
        });

        // paired (perf 0 + sibling), perf-only (perf 1), csv-only (Solo) => three groups.
        ASSERT_EQ(reports.size(), 3U);
        EXPECT_EQ(reports.back(), (std::pair<std::size_t, std::size_t>{3, 3}));
        EXPECT_EQ(profile.getAllRunRecords().size(), 3U);
    }

    TEST_F(ProfileBuilderTest, BuildRegistersBothPerfAndStatsSourceDirsForARootWithCsv) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100, 1.0F)};
        fake_file_service->stats_files = {stats_file("listed-perf-0 Stats.csv")};
        fake_file_service->stats_by_path.emplace("listed-perf-0 Stats.csv", csv_totals("hash-1", 1.0F));

        const auto profile = builder.build();

        const auto &entries = profile.sources().entries();
        EXPECT_TRUE(std::ranges::any_of(entries, [](const auto &e) { return e.path == "FPSAimTrainer/performances"; }));
        EXPECT_TRUE(std::ranges::any_of(entries, [](const auto &e) { return e.path == "FPSAimTrainer/stats"; }));
    }
}
