//
// ProfileV3Migrator tests. Builds v3 stores in-process with the retired
// profile_v3 descriptor and drives migration through a real RunIngestor.
//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "formats/protobuf/migration/profile_v3.pb.h"
#include "formats/protobuf/migration/profile_v3_migrator.h"
#include "formats/csv/stats_csv_parser.h"
#include "run_ingestor.h"
#include "fake_file_service.h"

using namespace ksv::data;
using ksv::tests_support::FakeFileService;

namespace {
    class ProfileV3MigratorTest : public testing::Test {
    protected:
        std::filesystem::path root = std::filesystem::temp_directory_path() / "ksv_v3_migration_root";
        std::filesystem::path v3_path = root / "profile_v3.pb";
        std::shared_ptr<FakeFileService> files = std::make_shared<FakeFileService>();
        ProfileV3Migrator migrator{std::make_shared<RunIngestor>(files)};

        void SetUp() override {
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
            files->source_roots = {root.generic_string()};
        }

        void TearDown() override { std::filesystem::remove_all(root); }

        static profile_v3::ScenarioDataPoint *addSample(profile_v3::Run *run, const float time, const int shots,
                                                        const int hits, const float score, const int kills) {
            auto *point = run->add_data();
            point->set_time(time);
            point->set_shots(shots);
            point->set_hits(hits);
            point->set_score(score);
            point->set_kills(kills);
            return point;
        }

        void writeV3(const profile_v3::ProfileStoreFile &file) const {
            std::ofstream out(v3_path, std::ios::out | std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(file.SerializeToOstream(&out));
        }
    };

    TEST_F(ProfileV3MigratorTest, RejectsUnsupportedFromVersion) {
        EXPECT_FALSE(migrator.migrate(v3_path, 2).has_value());
    }

    TEST_F(ProfileV3MigratorTest, RestoresSourcesRunIdentityAndDerivesFallbackTotalsOnce) {
        profile_v3::ProfileStoreFile file;
        file.mutable_header()->set_version(3);
        auto *store = file.mutable_store();
        auto *root_dir = store->add_sources();
        root_dir->set_id(1);
        root_dir->set_parent_id(0);
        root_dir->set_path(root.generic_string());
        auto *perf_dir = store->add_sources();
        perf_dir->set_id(2);
        perf_dir->set_parent_id(1);
        perf_dir->set_path("FPSAimTrainer/performances");

        auto *run = store->add_runs();
        run->mutable_scenario_id()->set_name("Old Scenario");
        run->mutable_scenario_id()->set_hash("old-hash");
        run->set_start_time(1700000000000);
        run->set_scenario_length(58.0F);
        run->set_source_directory_id(2);
        run->set_source_filename("Old Scenario Performance.perf");
        addSample(run, 0.5F, 3, 2, 10.0F, 2);
        addSample(run, 1.5F, 4, 3, 5.0F, 1);
        writeV3(file);

        const auto migrated = migrator.migrate(v3_path, 3);
        ASSERT_TRUE(migrated.has_value());

        EXPECT_EQ(migrated->sources().entries().size(), 2U);
        const auto runs = migrated->getRunsForScenario({.name = "Old Scenario", .hash = "old-hash"});
        ASSERT_EQ(runs.size(), 1U);
        const auto &out = runs.front();
        EXPECT_EQ(out.run_id.start_time, 1700000000000);
        EXPECT_FLOAT_EQ(out.scenario_length, 58.0F);
        ASSERT_TRUE(out.performance.has_value());
        ASSERT_EQ(out.performance->samples.size(), 2U);
        EXPECT_EQ(out.totals().shots, 7);
        EXPECT_EQ(out.totals().hits, 5);
        EXPECT_EQ(out.totals().kills, 3);
        EXPECT_FLOAT_EQ(out.totals().score, 15.0F);
        EXPECT_FALSE(out.stats.has_value());
        EXPECT_FALSE(out.sources.csv.has_value());
        ASSERT_TRUE(out.sources.perf.has_value());
    }

    TEST_F(ProfileV3MigratorTest, AttachesCsvSiblingStatsAndTotalsWhilePreservingPerfKey) {
        const std::filesystem::path fixture = std::filesystem::path(TEST_FILES_DIR) / "1wall6targets TE.csv";
        const auto parsed = StatsCsvParser{}.parseFile(fixture);
        ASSERT_TRUE(parsed.has_value());

        const auto stats_dir = root / "FPSAimTrainer" / "stats";
        std::filesystem::create_directories(stats_dir);
        std::filesystem::copy_file(fixture, stats_dir / "TE Stats.csv");
        files->stats_by_path["TE Stats.csv"] = *parsed;

        profile_v3::ProfileStoreFile file;
        file.mutable_header()->set_version(3);
        auto *store = file.mutable_store();
        auto *root_dir = store->add_sources();
        root_dir->set_id(1);
        root_dir->set_path(root.generic_string());
        auto *perf_dir = store->add_sources();
        perf_dir->set_id(2);
        perf_dir->set_parent_id(1);
        perf_dir->set_path("FPSAimTrainer/performances");

        auto *run = store->add_runs();
        run->mutable_scenario_id()->set_name("1wall6targets TE");
        run->mutable_scenario_id()->set_hash("3e50391f3c3f484c10a4b8fb362ded17");
        run->set_start_time(1699999999000);
        run->set_scenario_length(60.0F);
        run->set_source_directory_id(2);
        run->set_source_filename("TE Performance.perf");
        addSample(run, 1.0F, 1, 1, 1.0F, 1);
        writeV3(file);

        const auto migrated = migrator.migrate(v3_path, 3);
        ASSERT_TRUE(migrated.has_value());

        const auto runs = migrated->getRunsForScenario(
            {.name = "1wall6targets TE", .hash = "3e50391f3c3f484c10a4b8fb362ded17"});
        ASSERT_EQ(runs.size(), 1U);
        const auto &out = runs.front();
        EXPECT_EQ(out.run_id.start_time, 1699999999000);
        EXPECT_EQ(out.totals().shots, 180);
        EXPECT_EQ(out.totals().hits, 166);
        ASSERT_TRUE(out.stats.has_value());
        EXPECT_EQ(out.stats->sens_scale, "Valorant");
        ASSERT_TRUE(out.sources.csv.has_value());
        ASSERT_TRUE(out.sources.perf.has_value());
    }
}
