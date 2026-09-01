//
// Created by Lecka on 03/08/2026.
//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "formats/protobuf/profile_serializer.h"

#include "profile_store_files.h"

namespace {
    ksv::domain::Run make_perf(const std::string &name, const std::string &hash,
                                        const long long start_time, const float scenario_length,
                                        const ksv::domain::SourceFileRef source = {}) {
        ksv::domain::Run perf;
        perf.run_id.scenario_id.name = name;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = scenario_length;
        perf.sources.perf = source;
        perf.performance.emplace();
        perf.performance->add_data(0.0F, ksv::domain::SHOTS, 5);
        perf.performance->add_data(0.0F, ksv::domain::HITS, 4);
        perf.performance->add_data(1.5F, ksv::domain::SCORE, 42.0F);
        perf.stored_totals = {.score = 42.0F, .shots = 5, .hits = 4};
        return perf;
    }

    ksv::domain::Stats make_stats() {
        return {
            .sens_scale = "Valorant",
            .horiz_sens = 0.31F,
            .vert_sens = 0.31F,
            .dpi = 1600,
            .fov = 103.0F,
            .fov_scale = "Overwatch",
            .resolution = "1920x1080",
            .resolution_scale = 100.0F,
            .avg_fps = 240.5F,
        };
    }

    class ProfileSerializerTest : public testing::Test {
    protected:
        ksv::data::ProfileSerializer serializer;
        std::filesystem::path store_path =
            std::filesystem::temp_directory_path() / "profile_serializer_test_store.pb";

        [[nodiscard]] std::filesystem::path temp_path() const {
            auto path = store_path;
            path += ".tmp";
            return path;
        }

        [[nodiscard]] std::vector<std::filesystem::path> quarantine_files(const std::string &reason) const {
            return ksv::tests_support::quarantineFiles(store_path, reason);
        }

        void TearDown() override {
            std::filesystem::remove(store_path);
            std::filesystem::remove(temp_path());
            for (const auto &reason: {"unparseable", "version-mismatch"}) {
                for (const auto &path: quarantine_files(reason)) std::filesystem::remove(path);
            }
        }
    };

    TEST_F(ProfileSerializerTest, LoadReturnsNulloptWhenFileDoesNotExist) {
        std::filesystem::remove(store_path);
        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                  ksv::application::ProfileLoadError::NotFound);
    }

    TEST_F(ProfileSerializerTest, RoundTripsScenariosRunsAndDataPoints) {
        ksv::domain::UserProfile profile{ksv::domain::SourceRegistry{{
            {{7}, {}, "C:/Kovaaks"},
            {{12}, {7}, "FPSAimTrainer/performances"}
        }}};
        profile.addRun(make_perf("Scenario A", "hash-a", 100, 60.0F, {{12}, "run1.perf"}));
        profile.addRun(make_perf("Scenario A", "hash-a", 200, 60.0F, {{12}, "run2.perf"}));
        profile.addRun(make_perf("Scenario B", "hash-b", 300, 30.0F, {{12}, "run3.perf"}));

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto loaded = serializer.load(store_path);

        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        const auto& loaded_profile = std::get<ksv::domain::UserProfile>(loaded);
        EXPECT_EQ(loaded_profile.sources().entries(), profile.sources().entries());
        EXPECT_EQ(loaded_profile.getScenarioList().size(), 2);

        const auto scenario_a = ksv::domain::ScenarioId{.name = "Scenario A", .hash = "hash-a"};
        const auto runs_a = loaded_profile.getMostRecentRuns(scenario_a, 10);
        ASSERT_EQ(runs_a.size(), 2);
        EXPECT_EQ(runs_a[0].run_id.start_time, 100LL);
        EXPECT_EQ(runs_a[1].run_id.start_time, 200LL);
        EXPECT_FLOAT_EQ(runs_a[1].scenario_length, 60.0F);
        ASSERT_TRUE(runs_a[1].sources.perf.has_value());
        EXPECT_EQ(*runs_a[1].sources.perf, (ksv::domain::SourceFileRef{{12}, "run2.perf"}));
        EXPECT_EQ(loaded_profile.sources().resolve(*runs_a[1].sources.perf),
                  "C:/Kovaaks/FPSAimTrainer/performances/run2.perf");

        ASSERT_TRUE(runs_a[1].performance.has_value());
        ASSERT_EQ(runs_a[1].performance->samples.size(), 2);
        EXPECT_EQ(runs_a[1].performance->samples[0].shots, 5);
        EXPECT_EQ(runs_a[1].performance->samples[0].hits, 4);
        EXPECT_FLOAT_EQ(runs_a[1].performance->samples[1].time, 1.5F);
        EXPECT_FLOAT_EQ(runs_a[1].performance->samples[1].score, 42.0F);
        EXPECT_FLOAT_EQ(runs_a[1].totals().score, 42.0F);

        const auto scenario_b = ksv::domain::ScenarioId{.name = "Scenario B", .hash = "hash-b"};
        const auto perf_b = loaded_profile.getMostRecentRun(scenario_b);
        ASSERT_TRUE(perf_b.has_value());
        EXPECT_FLOAT_EQ(perf_b->scenario_length, 30.0F);
    }

    TEST_F(ProfileSerializerTest, RoundTripsAllThreeRunShapes) {
        ksv::domain::UserProfile profile{ksv::domain::SourceRegistry{{
            {{1}, {}, "C:/Kovaaks"},
            {{2}, {1}, "FPSAimTrainer/performances"},
            {{3}, {1}, "FPSAimTrainer/stats"}
        }}};

        ksv::domain::Run paired;
        paired.run_id = {{"Paired", "hash-paired"}, 1000};
        paired.scenario_length = 60.0F;
        paired.stored_totals = {.score = 12.5F, .shots = 8, .hits = 6, .misses = 2, .kills = 5};
        paired.sources.perf = {{2}, "paired.perf"};
        paired.sources.csv = {{3}, "paired.csv"};
        paired.performance.emplace();
        paired.performance->add_data(0.5F, ksv::domain::SHOTS, 3);
        paired.performance->add_data(0.5F, ksv::domain::SCORE, 7.0F);
        paired.stats = make_stats();

        ksv::domain::Run csv_only;
        csv_only.run_id = {{"CsvOnly", "hash-csv"}, 2000};
        csv_only.scenario_length = 59.8F;
        csv_only.stored_totals = {.score = 99.0F, .shots = 10, .hits = 9, .misses = 1, .kills = 9};
        csv_only.sources.csv = {{3}, "csvonly.csv"};
        csv_only.stats = make_stats();

        ksv::domain::Run perf_only;
        perf_only.run_id = {{"PerfOnly", "hash-perf"}, 3000};
        perf_only.scenario_length = 30.0F;
        perf_only.stored_totals = {.score = 5.0F, .shots = 4, .hits = 3, .misses = 1, .kills = 0};
        perf_only.sources.perf = {{2}, "perfonly.perf"};
        perf_only.performance.emplace();
        perf_only.performance->add_data(1.0F, ksv::domain::HITS, 3);

        profile.addRun(paired);
        profile.addRun(csv_only);
        profile.addRun(perf_only);

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto loaded = serializer.load(store_path);
        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        const auto &out = std::get<ksv::domain::UserProfile>(loaded);

        const auto loaded_paired = out.getMostRecentRun({"Paired", "hash-paired"});
        ASSERT_TRUE(loaded_paired.has_value());
        EXPECT_EQ(loaded_paired->run_id, paired.run_id);
        EXPECT_FLOAT_EQ(loaded_paired->scenario_length, 60.0F);
        EXPECT_EQ(loaded_paired->totals(), paired.stored_totals);
        ASSERT_TRUE(loaded_paired->sources.perf.has_value());
        ASSERT_TRUE(loaded_paired->sources.csv.has_value());
        EXPECT_EQ(*loaded_paired->sources.perf, (ksv::domain::SourceFileRef{{2}, "paired.perf"}));
        EXPECT_EQ(*loaded_paired->sources.csv, (ksv::domain::SourceFileRef{{3}, "paired.csv"}));
        ASSERT_TRUE(loaded_paired->performance.has_value());
        ASSERT_EQ(loaded_paired->performance->samples.size(), 1U);
        EXPECT_FLOAT_EQ(loaded_paired->performance->samples[0].time, 0.5F);
        EXPECT_EQ(loaded_paired->performance->samples[0].shots, 3);
        EXPECT_FLOAT_EQ(loaded_paired->performance->samples[0].score, 7.0F);
        ASSERT_TRUE(loaded_paired->stats.has_value());
        EXPECT_EQ(*loaded_paired->stats, make_stats());

        const auto loaded_csv = out.getMostRecentRun({"CsvOnly", "hash-csv"});
        ASSERT_TRUE(loaded_csv.has_value());
        EXPECT_EQ(loaded_csv->totals(), csv_only.stored_totals);
        EXPECT_FALSE(loaded_csv->performance.has_value());
        EXPECT_FALSE(loaded_csv->sources.perf.has_value());
        ASSERT_TRUE(loaded_csv->sources.csv.has_value());
        EXPECT_EQ(*loaded_csv->sources.csv, (ksv::domain::SourceFileRef{{3}, "csvonly.csv"}));
        ASSERT_TRUE(loaded_csv->stats.has_value());
        EXPECT_EQ(*loaded_csv->stats, make_stats());

        const auto loaded_perf = out.getMostRecentRun({"PerfOnly", "hash-perf"});
        ASSERT_TRUE(loaded_perf.has_value());
        EXPECT_EQ(loaded_perf->totals(), perf_only.stored_totals);
        EXPECT_FALSE(loaded_perf->stats.has_value());
        EXPECT_FALSE(loaded_perf->sources.csv.has_value());
        ASSERT_TRUE(loaded_perf->sources.perf.has_value());
        ASSERT_TRUE(loaded_perf->performance.has_value());
        ASSERT_EQ(loaded_perf->performance->samples.size(), 1U);
        EXPECT_EQ(loaded_perf->performance->samples[0].hits, 3);
    }

    TEST_F(ProfileSerializerTest, RoundTripOfEmptyProfileHasNoScenarios) {
        const ksv::domain::UserProfile profile;

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto loaded = serializer.load(store_path);

        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        EXPECT_TRUE(std::get<ksv::domain::UserProfile>(loaded).getScenarioList().empty());
    }

    TEST_F(ProfileSerializerTest, ReadHeaderReturnsMetadataWrittenBySave) {
        const ksv::domain::UserProfile profile;
        ASSERT_TRUE(serializer.save(profile, store_path));

        const auto header = serializer.readHeader(store_path);

        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->version, 4U);
        EXPECT_GT(header->created_at, 0);
        EXPECT_EQ(header->name, "default");
    }

    TEST_F(ProfileSerializerTest, SavePreservesExistingHeaderMetadata) {
        const ksv::domain::UserProfile profile;
        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto first_header = serializer.readHeader(store_path);
        ASSERT_TRUE(first_header.has_value());

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto second_header = serializer.readHeader(store_path);

        ASSERT_TRUE(second_header.has_value());
        EXPECT_EQ(second_header->created_at, first_header->created_at);
        EXPECT_EQ(second_header->name, first_header->name);
    }

    TEST_F(ProfileSerializerTest, SaveLeavesNoTemporaryFile) {
        const ksv::domain::UserProfile profile;

        ASSERT_TRUE(serializer.save(profile, store_path));

        EXPECT_FALSE(std::filesystem::exists(temp_path()));
    }

    TEST_F(ProfileSerializerTest, FailedSavePreservesExistingStore) {
        const ksv::domain::UserProfile profile;
        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto original_bytes = ksv::tests_support::readFile(store_path);
        ASSERT_TRUE(std::filesystem::create_directory(temp_path()));

        EXPECT_FALSE(serializer.save(profile, store_path));
        EXPECT_EQ(ksv::tests_support::readFile(store_path), original_bytes);
        EXPECT_FALSE(std::filesystem::exists(temp_path()));
    }

    TEST_F(ProfileSerializerTest, SaveReplacesLeftoverTemporaryFile) {
        const ksv::domain::UserProfile profile;
        {
            std::ofstream output(temp_path(), std::ios::out | std::ios::binary | std::ios::trunc);
            output << "junk";
        }

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto loaded = serializer.load(store_path);

        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        EXPECT_TRUE(std::get<ksv::domain::UserProfile>(loaded).getScenarioList().empty());
    }

    TEST_F(ProfileSerializerTest, ReadHeaderReturnsNulloptWithoutQuarantiningInvalidFiles) {
        EXPECT_FALSE(serializer.readHeader(store_path).has_value());

        const std::string garbage{"\x00\x01\x02", 3};
        std::ofstream output(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        output.write(garbage.data(), static_cast<std::streamsize>(garbage.size()));
        output.close();

        EXPECT_FALSE(serializer.readHeader(store_path).has_value());
        EXPECT_TRUE(std::filesystem::exists(store_path));
        EXPECT_EQ(ksv::tests_support::readFile(store_path), garbage);
    }

    TEST_F(ProfileSerializerTest, ReadHeaderDoesNotParseTheStoreBody) {
        profile::File file;
        file.mutable_header()->set_version(4);
        file.mutable_header()->set_created_at(1);
        file.mutable_header()->set_name("default");
        std::ofstream output(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.SerializeToOstream(&output);
        const std::string trailing_garbage{"\x00\x01\x02", 3};
        output.write(trailing_garbage.data(), static_cast<std::streamsize>(trailing_garbage.size()));
        output.close();

        const auto header = serializer.readHeader(store_path);

        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->version, 4U);
        EXPECT_EQ(header->created_at, 1);
        EXPECT_EQ(header->name, "default");
    }

    TEST_F(ProfileSerializerTest, LoadReturnsNulloptForUnparseableFile) {
        // A leading tag byte of 0x00 encodes field number 0, which is illegal in
        // protobuf's wire format and is guaranteed to fail parsing.
        const char garbage_bytes[] = {0x00, 0x01, 0x02, 0x03};
        std::ofstream garbage(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        garbage.write(garbage_bytes, sizeof(garbage_bytes));
        garbage.close();

        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                  ksv::application::ProfileLoadError::Unparseable);
        EXPECT_FALSE(std::filesystem::exists(store_path));
        const auto quarantined = quarantine_files("unparseable");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(ksv::tests_support::readFile(quarantined.front()), std::string(garbage_bytes, sizeof(garbage_bytes)));
    }

    TEST_F(ProfileSerializerTest, LoadRejectsStoreWithMismatchedSchemaVersion) {
        profile::File file;
        file.mutable_header()->set_version(1);
        auto *run = file.mutable_store()->add_runs();
        run->mutable_scenario_id()->set_name("Scenario A");
        run->mutable_scenario_id()->set_hash("hash-a");
        run->set_start_time(100);

        std::ofstream out(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.SerializeToOstream(&out);
        out.close();
        const auto original_bytes = ksv::tests_support::readFile(store_path);

        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                  ksv::application::ProfileLoadError::VersionMismatch);
        EXPECT_FALSE(std::filesystem::exists(store_path));
        const auto quarantined = quarantine_files("version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(ksv::tests_support::readFile(quarantined.front()), original_bytes);
    }

    class StubMigrator final : public ksv::data::IProfileMigrator {
    public:
        std::uint32_t seen_from_version = 0;
        bool succeed = true;

        [[nodiscard]] std::optional<ksv::domain::UserProfile> migrate(
            const std::filesystem::path &, const std::uint32_t from_version) const override {
            const_cast<StubMigrator *>(this)->seen_from_version = from_version;
            if (!succeed) return std::nullopt;
            ksv::domain::UserProfile profile{ksv::domain::SourceRegistry{{{{1}, {}, "C:/Kovaaks"}}}};
            ksv::domain::Run run;
            run.run_id = {{"Migrated", "hash-m"}, 42};
            run.scenario_length = 30.0F;
            run.stored_totals = {.score = 7.0F, .shots = 2, .hits = 2};
            run.sources.perf = {{1}, "m.perf"};
            profile.addRun(run);
            return profile;
        }
    };

    TEST_F(ProfileSerializerTest, DelegatesLegacyVersionToMigratorThenRewritesAtomically) {
        profile::File legacy;
        legacy.mutable_header()->set_version(3);
        legacy.mutable_header()->set_created_at(11);
        legacy.mutable_header()->set_name("legacy");
        std::ofstream out(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        legacy.SerializeToOstream(&out);
        out.close();

        auto stub = std::make_shared<StubMigrator>();
        ksv::data::ProfileSerializer migrating{stub};

        const auto loaded = migrating.load(store_path);

        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        EXPECT_EQ(stub->seen_from_version, 3U);
        EXPECT_EQ(std::get<ksv::domain::UserProfile>(loaded).getScenarioList().size(), 1U);

        // The v3 file on disk is replaced by a readable v4 store.
        const auto header = migrating.readHeader(store_path);
        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->version, 4U);
        EXPECT_FALSE(std::filesystem::exists(temp_path()));
        EXPECT_TRUE(quarantine_files("version-mismatch").empty());

        const auto reloaded = migrating.load(store_path);
        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(reloaded));
        EXPECT_EQ(std::get<ksv::domain::UserProfile>(reloaded).getScenarioList().size(), 1U);
    }

    TEST_F(ProfileSerializerTest, QuarantinesWhenMigratorCannotApply) {
        profile::File legacy;
        legacy.mutable_header()->set_version(3);
        std::ofstream out(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        legacy.SerializeToOstream(&out);
        out.close();
        const auto original_bytes = ksv::tests_support::readFile(store_path);

        auto stub = std::make_shared<StubMigrator>();
        stub->succeed = false;
        ksv::data::ProfileSerializer migrating{stub};

        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(migrating.load(store_path)),
                  ksv::application::ProfileLoadError::VersionMismatch);
        const auto quarantined = quarantine_files("version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(ksv::tests_support::readFile(quarantined.front()), original_bytes);
    }

    TEST_F(ProfileSerializerTest, LoadQuarantinesLegacyStoreAsUnparseable) {
        profile::Run legacy_run;
        legacy_run.mutable_scenario_id()->set_name("Scenario A");
        legacy_run.mutable_scenario_id()->set_hash("hash-a");
        legacy_run.set_start_time(100);
        const auto run_bytes = legacy_run.SerializeAsString();
        ASSERT_LT(run_bytes.size(), 128U);

        std::ofstream out(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        out.put(0x12);
        out.put(static_cast<char>(run_bytes.size()));
        out.write(run_bytes.data(), static_cast<std::streamsize>(run_bytes.size()));
        out.close();

        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                  ksv::application::ProfileLoadError::Unparseable);
        EXPECT_FALSE(std::filesystem::exists(store_path));
        EXPECT_EQ(quarantine_files("unparseable").size(), 1);
    }

    TEST_F(ProfileSerializerTest, SuccessiveDistinctRejectionsPreserveBothFiles) {
        const std::string first_bytes{"\x00\x01\x02", 3};
        const std::string second_bytes{"\x00\x03\x04", 3};

        for (const auto &bytes: {first_bytes, second_bytes}) {
            std::ofstream output(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
            EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                      ksv::application::ProfileLoadError::Unparseable);
        }

        const auto quarantined = quarantine_files("unparseable");
        ASSERT_EQ(quarantined.size(), 2);
        std::set<std::string> preserved;
        for (const auto &path: quarantined) preserved.insert(ksv::tests_support::readFile(path));
        EXPECT_EQ(preserved, (std::set<std::string>{first_bytes, second_bytes}));
    }
}
