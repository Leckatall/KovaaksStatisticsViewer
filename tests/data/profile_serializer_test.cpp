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

namespace {
    std::string read_file(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    ksv::domain::ScenarioPerf make_perf(const std::string &name, const std::string &hash,
                                        const long long start_time, const float scenario_length,
                                        const ksv::domain::SourceFileRef source = {}) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = name;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = scenario_length;
        perf.source = source;
        perf.add_data(0.0F, ksv::domain::SHOTS, 5);
        perf.add_data(0.0F, ksv::domain::HITS, 4);
        perf.add_data(1.5F, ksv::domain::SCORE, 42.0F);
        return perf;
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
            std::vector<std::filesystem::path> matches;
            const auto prefix = store_path.stem().string() + "_" + reason + "_";
            for (const auto &entry: std::filesystem::directory_iterator(store_path.parent_path())) {
                if (entry.path().extension() == store_path.extension() &&
                    entry.path().stem().string().starts_with(prefix)) {
                    matches.push_back(entry.path());
                }
            }
            return matches;
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
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 100, 60.0F, {{12}, "run1.perf"}));
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 200, 60.0F, {{12}, "run2.perf"}));
        profile.addScenarioPerf(make_perf("Scenario B", "hash-b", 300, 30.0F, {{12}, "run3.perf"}));

        ASSERT_TRUE(serializer.save(profile, store_path));
        const auto loaded = serializer.load(store_path);

        ASSERT_TRUE(std::holds_alternative<ksv::domain::UserProfile>(loaded));
        const auto& loaded_profile = std::get<ksv::domain::UserProfile>(loaded);
        EXPECT_EQ(loaded_profile.sources().entries(), profile.sources().entries());
        EXPECT_EQ(loaded_profile.getScenarioList().size(), 2);

        const auto scenario_a = ksv::domain::ScenarioId{.name = "Scenario A", .hash = "hash-a"};
        const auto runs_a = loaded_profile.getMostRecentPerfs(scenario_a, 10);
        ASSERT_EQ(runs_a.size(), 2);
        EXPECT_EQ(runs_a[0].run_id.start_time, 100LL);
        EXPECT_EQ(runs_a[1].run_id.start_time, 200LL);
        EXPECT_FLOAT_EQ(runs_a[1].scenario_length, 60.0F);
        EXPECT_EQ(runs_a[1].source, (ksv::domain::SourceFileRef{{12}, "run2.perf"}));
        EXPECT_EQ(loaded_profile.sources().resolve(runs_a[1].source),
                  "C:/Kovaaks/FPSAimTrainer/performances/run2.perf");

        ASSERT_EQ(runs_a[1].data.size(), 2);
        EXPECT_EQ(runs_a[1].data[0].shots, 5);
        EXPECT_EQ(runs_a[1].data[0].hits, 4);
        EXPECT_FLOAT_EQ(runs_a[1].data[1].time, 1.5F);
        EXPECT_FLOAT_EQ(runs_a[1].data[1].score, 42.0F);

        const auto scenario_b = ksv::domain::ScenarioId{.name = "Scenario B", .hash = "hash-b"};
        const auto perf_b = loaded_profile.getMostRecentPerf(scenario_b);
        ASSERT_TRUE(perf_b.has_value());
        EXPECT_FLOAT_EQ(perf_b->scenario_length, 30.0F);
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
        EXPECT_EQ(header->version, 3U);
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
        const auto original_bytes = read_file(store_path);
        ASSERT_TRUE(std::filesystem::create_directory(temp_path()));

        EXPECT_FALSE(serializer.save(profile, store_path));
        EXPECT_EQ(read_file(store_path), original_bytes);
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
        EXPECT_EQ(read_file(store_path), garbage);
    }

    TEST_F(ProfileSerializerTest, ReadHeaderDoesNotParseTheStoreBody) {
        store::ProfileStoreFile file;
        file.mutable_header()->set_version(3);
        file.mutable_header()->set_created_at(1);
        file.mutable_header()->set_name("default");
        std::ofstream output(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.SerializeToOstream(&output);
        const std::string trailing_garbage{"\x00\x01\x02", 3};
        output.write(trailing_garbage.data(), static_cast<std::streamsize>(trailing_garbage.size()));
        output.close();

        const auto header = serializer.readHeader(store_path);

        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->version, 3U);
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
        EXPECT_EQ(read_file(quarantined.front()), std::string(garbage_bytes, sizeof(garbage_bytes)));
    }

    TEST_F(ProfileSerializerTest, LoadRejectsStoreWithMismatchedSchemaVersion) {
        store::ProfileStoreFile file;
        file.mutable_header()->set_version(1);
        auto *run = file.mutable_store()->add_runs();
        run->mutable_scenario_id()->set_name("Scenario A");
        run->mutable_scenario_id()->set_hash("hash-a");
        run->set_start_time(100);

        std::ofstream out(store_path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.SerializeToOstream(&out);
        out.close();
        const auto original_bytes = read_file(store_path);

        EXPECT_EQ(std::get<ksv::application::ProfileLoadError>(serializer.load(store_path)),
                  ksv::application::ProfileLoadError::VersionMismatch);
        EXPECT_FALSE(std::filesystem::exists(store_path));
        const auto quarantined = quarantine_files("version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(read_file(quarantined.front()), original_bytes);
    }

    TEST_F(ProfileSerializerTest, LoadQuarantinesLegacyStoreAsUnparseable) {
        store::Run legacy_run;
        auto *run = &legacy_run;
        run->mutable_scenario_id()->set_name("Scenario A");
        run->mutable_scenario_id()->set_hash("hash-a");
        run->set_start_time(100);
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
        for (const auto &path: quarantined) preserved.insert(read_file(path));
        EXPECT_EQ(preserved, (std::set<std::string>{first_bytes, second_bytes}));
    }
}
