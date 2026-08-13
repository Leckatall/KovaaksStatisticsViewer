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
                                        const std::string &source_file = {}) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = name;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = scenario_length;
        perf.source_file = source_file;
        perf.add_data(0.0F, ksv::domain::SHOTS, 5);
        perf.add_data(0.0F, ksv::domain::HITS, 4);
        perf.add_data(1.5F, ksv::domain::SCORE, 42.0F);
        return perf;
    }

    class ProfileSerializerTest : public testing::Test {
    protected:
        ksv::data::ProfileSerializer serializer;
        std::filesystem::path cache_path =
            std::filesystem::temp_directory_path() / "profile_serializer_test_cache.pb";

        [[nodiscard]] std::vector<std::filesystem::path> quarantine_files(const std::string &reason) const {
            std::vector<std::filesystem::path> matches;
            const auto prefix = cache_path.stem().string() + "_" + reason + "_";
            for (const auto &entry: std::filesystem::directory_iterator(cache_path.parent_path())) {
                if (entry.path().extension() == cache_path.extension() &&
                    entry.path().stem().string().starts_with(prefix)) {
                    matches.push_back(entry.path());
                }
            }
            return matches;
        }

        void TearDown() override {
            std::filesystem::remove(cache_path);
            for (const auto &reason: {"unparseable", "version-mismatch"}) {
                for (const auto &path: quarantine_files(reason)) std::filesystem::remove(path);
            }
        }
    };

    TEST_F(ProfileSerializerTest, LoadReturnsNulloptWhenFileDoesNotExist) {
        std::filesystem::remove(cache_path);
        EXPECT_FALSE(serializer.load(cache_path).has_value());
    }

    TEST_F(ProfileSerializerTest, RoundTripsScenariosRunsAndDataPoints) {
        ksv::domain::UserProfile profile{"C:/Kovaaks/FPSAimTrainer/performances"};
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 100, 60.0F, "C:/Kovaaks/.../run1.perf"));
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 200, 60.0F, "C:/Kovaaks/.../run2.perf"));
        profile.addScenarioPerf(make_perf("Scenario B", "hash-b", 300, 30.0F, "C:/Kovaaks/.../run3.perf"));

        serializer.save(profile, cache_path);
        const auto loaded = serializer.load(cache_path);

        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->getSourceDirectory(), "C:/Kovaaks/FPSAimTrainer/performances");
        EXPECT_EQ(loaded->getScenarioList().size(), 2);

        const auto scenario_a = ksv::domain::ScenarioId{.name = "Scenario A", .hash = "hash-a"};
        const auto runs_a = loaded->getMostRecentPerfs(scenario_a, 10);
        ASSERT_EQ(runs_a.size(), 2);
        EXPECT_EQ(runs_a[0].run_id.start_time, 100LL);
        EXPECT_EQ(runs_a[1].run_id.start_time, 200LL);
        EXPECT_FLOAT_EQ(runs_a[1].scenario_length, 60.0F);
        EXPECT_EQ(runs_a[1].source_file, "C:/Kovaaks/.../run2.perf");

        ASSERT_EQ(runs_a[1].data.size(), 2);
        EXPECT_EQ(runs_a[1].data[0].shots, 5);
        EXPECT_EQ(runs_a[1].data[0].hits, 4);
        EXPECT_FLOAT_EQ(runs_a[1].data[1].time, 1.5F);
        EXPECT_FLOAT_EQ(runs_a[1].data[1].score, 42.0F);

        const auto scenario_b = ksv::domain::ScenarioId{.name = "Scenario B", .hash = "hash-b"};
        const auto perf_b = loaded->getMostRecentPerf(scenario_b);
        ASSERT_TRUE(perf_b.has_value());
        EXPECT_FLOAT_EQ(perf_b->scenario_length, 30.0F);
    }

    TEST_F(ProfileSerializerTest, RoundTripOfEmptyProfileHasNoScenarios) {
        const ksv::domain::UserProfile profile{"default"};

        serializer.save(profile, cache_path);
        const auto loaded = serializer.load(cache_path);

        ASSERT_TRUE(loaded.has_value());
        EXPECT_TRUE(loaded->getScenarioList().empty());
    }

    TEST_F(ProfileSerializerTest, LoadReturnsNulloptForUnparseableFile) {
        // A leading tag byte of 0x00 encodes field number 0, which is illegal in
        // protobuf's wire format and is guaranteed to fail parsing.
        const char garbage_bytes[] = {0x00, 0x01, 0x02, 0x03};
        std::ofstream garbage(cache_path, std::ios::out | std::ios::binary | std::ios::trunc);
        garbage.write(garbage_bytes, sizeof(garbage_bytes));
        garbage.close();

        EXPECT_FALSE(serializer.load(cache_path).has_value());
        EXPECT_FALSE(std::filesystem::exists(cache_path));
        const auto quarantined = quarantine_files("unparseable");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(read_file(quarantined.front()), std::string(garbage_bytes, sizeof(garbage_bytes)));
    }

    TEST_F(ProfileSerializerTest, LoadRejectsCacheWithMismatchedSchemaVersion) {
        // A cache written by an incompatible (or pre-versioning) schema reads
        // back with a version that doesn't match the current one; load() must
        // reject it so the caller regenerates instead of loading a
        // silently-empty/mis-parsed profile. Version 0 stands in for such a
        // cache (the current writer always stamps a non-zero version).
        cache::UserProfileCache proto;
        proto.set_version(0);
        proto.set_source_directory("C:/Kovaaks/FPSAimTrainer/performances");
        auto *run = proto.add_runs();
        run->mutable_scenario_id()->set_name("Scenario A");
        run->mutable_scenario_id()->set_hash("hash-a");
        run->set_start_time(100);

        std::ofstream out(cache_path, std::ios::out | std::ios::binary | std::ios::trunc);
        proto.SerializeToOstream(&out);
        out.close();
        const auto original_bytes = read_file(cache_path);

        EXPECT_FALSE(serializer.load(cache_path).has_value());
        EXPECT_FALSE(std::filesystem::exists(cache_path));
        const auto quarantined = quarantine_files("version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(read_file(quarantined.front()), original_bytes);
    }

    TEST_F(ProfileSerializerTest, SuccessiveDistinctRejectionsPreserveBothFiles) {
        const std::string first_bytes{"\x00\x01\x02", 3};
        const std::string second_bytes{"\x00\x03\x04", 3};

        for (const auto &bytes: {first_bytes, second_bytes}) {
            std::ofstream output(cache_path, std::ios::out | std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
            EXPECT_FALSE(serializer.load(cache_path).has_value());
        }

        const auto quarantined = quarantine_files("unparseable");
        ASSERT_EQ(quarantined.size(), 2);
        std::set<std::string> preserved;
        for (const auto &path: quarantined) preserved.insert(read_file(path));
        EXPECT_EQ(preserved, (std::set<std::string>{first_bytes, second_bytes}));
    }
}
