//
// Created by Lecka on 03/08/2026.
//

#include <gtest/gtest.h>

#include <filesystem>

#include "formats/protobuf/profile_serializer.h"

namespace {
    ksv::domain::ScenarioPerf make_perf(const std::string &name, const std::string &hash,
                                        const long long start_time, const float scenario_length) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = name;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = scenario_length;
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

        void TearDown() override {
            std::filesystem::remove(cache_path);
        }
    };

    TEST_F(ProfileSerializerTest, LoadReturnsNulloptWhenFileDoesNotExist) {
        std::filesystem::remove(cache_path);
        EXPECT_FALSE(serializer.load(cache_path).has_value());
    }

    TEST_F(ProfileSerializerTest, RoundTripsScenariosRunsAndDataPoints) {
        ksv::domain::UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 100, 60.0F));
        profile.addScenarioPerf(make_perf("Scenario A", "hash-a", 200, 60.0F));
        profile.addScenarioPerf(make_perf("Scenario B", "hash-b", 300, 30.0F));

        serializer.save(profile, cache_path);
        const auto loaded = serializer.load(cache_path);

        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->getScenarioList().size(), 2);

        const auto scenario_a = ksv::domain::ScenarioId{.name = "Scenario A", .hash = "hash-a"};
        const auto runs_a = loaded->getMostRecentPerfs(scenario_a, 10);
        ASSERT_EQ(runs_a.size(), 2);
        EXPECT_EQ(runs_a[0].run_id.start_time, 100LL);
        EXPECT_EQ(runs_a[1].run_id.start_time, 200LL);
        EXPECT_FLOAT_EQ(runs_a[1].scenario_length, 60.0F);

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
}
