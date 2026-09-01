#include <gtest/gtest.h>

#include <chrono>
#include <map>

#include "gallery_dataset.h"

namespace {
    using namespace std::chrono;
    using ksv::gallery::Dataset;

    const auto kPrimary = ksv::domain::ScenarioId{
        .name = "1wall6targets Gallery",
        .hash = "gallery-primary",
    };

    TEST(GalleryDatasetTest, NamesExposeEverySwitchableStateInDisplayOrder) {
        EXPECT_EQ(ksv::gallery::kDatasetNames,
                  (std::array{std::string_view{"Rich profile"}, std::string_view{"Single run"},
                              std::string_view{"Short history"}, std::string_view{"Bunched history"}}));
    }

    TEST(GalleryDatasetTest, RichProfileDefaultsToManyPrimaryRunsAndSeveralScenarios) {
        const auto profile = ksv::gallery::makeProfile(Dataset::RichProfile);

        EXPECT_GE(profile.getScenarioList().size(), 4U);
        ASSERT_TRUE(profile.getRunCount(kPrimary));
        EXPECT_GE(*profile.getRunCount(kPrimary), 30U);
        EXPECT_GE(profile.getAllRunRecords().size(), 60U);

        const auto latest = profile.getLatestRun();
        ASSERT_TRUE(latest);
        EXPECT_EQ(latest->run_id.scenario_id, kPrimary);
        ASSERT_TRUE(latest->performance);
        EXPECT_GE(latest->performance->samples.size(), 20U);

        const auto runs = profile.getRunsForScenario(kPrimary);
        ASSERT_GE(runs.size(), 2U);
        EXPECT_GE(runs.back().run_id.startDay() - runs.front().run_id.startDay(), days{20});
    }

    TEST(GalleryDatasetTest, SingleRunStillContainsPerformanceSamples) {
        const auto profile = ksv::gallery::makeProfile(Dataset::SingleRun);

        ASSERT_EQ(profile.getAllRunRecords().size(), 1U);
        const auto &run = profile.getAllRunRecords().front();
        ASSERT_TRUE(run.performance);
        EXPECT_FLOAT_EQ(run.scenario_length, 60.0F);
        EXPECT_EQ(run.performance->samples.size(), 60U);

        ksv::domain::RunTotals derived_totals;
        bool has_fractional_accuracy = false;
        for (const auto &sample: run.performance->samples) {
            EXPECT_GE(sample.time, 0.0F);
            EXPECT_LT(sample.time, run.scenario_length);
            EXPECT_GE(sample.shots, 3);
            EXPECT_LE(sample.shots, 10);
            EXPECT_EQ(sample.shots, sample.hits + sample.misses);
            EXPECT_LE(sample.kills, sample.hits);
            EXPECT_GE(sample.score, 0.0F);
            EXPECT_GE(sample.dmg, 0.0F);
            EXPECT_LE(sample.dmg, sample.dmg_possible);
            if (sample.hits == 0) {
                EXPECT_FLOAT_EQ(sample.score, 0.0F);
                EXPECT_FLOAT_EQ(sample.dmg, 0.0F);
                EXPECT_EQ(sample.kills, 0);
            }
            has_fractional_accuracy |= sample.hits > 0 && sample.hits < sample.shots;

            derived_totals.score += sample.score;
            derived_totals.shots += sample.shots;
            derived_totals.hits += sample.hits;
            derived_totals.misses += sample.misses;
            derived_totals.kills += sample.kills;
        }

        EXPECT_TRUE(has_fractional_accuracy);
        EXPECT_EQ(run.stored_totals.shots, derived_totals.shots);
        EXPECT_EQ(run.stored_totals.hits, derived_totals.hits);
        EXPECT_EQ(run.stored_totals.misses, derived_totals.misses);
        EXPECT_EQ(run.stored_totals.kills, derived_totals.kills);
        EXPECT_NEAR(run.stored_totals.score, derived_totals.score, 0.01F);
        EXPECT_GE(run.stored_totals.accuracy(), 0.5);
        EXPECT_LE(run.stored_totals.accuracy(), 0.95);
    }

    TEST(GalleryDatasetTest, ShortHistoryContainsFourRunsOfThePrimaryScenario) {
        const auto profile = ksv::gallery::makeProfile(Dataset::ShortHistory);

        EXPECT_EQ(profile.getCompletionHistory(kPrimary).size(), 4U);
        EXPECT_EQ(profile.getAllRunRecords().size(), 4U);
    }

    TEST(GalleryDatasetTest, BunchedHistoryHasFiveHoursThenFourEmptyDaysThenOneHour) {
        const auto profile = ksv::gallery::makeProfile(Dataset::BunchedHistory);
        std::map<sys_days, double> seconds_by_day;
        for (const auto &run: profile.getAllRunRecords()) {
            EXPECT_FLOAT_EQ(run.scenario_length, 60.0F);
            seconds_by_day[run.run_id.startDay()] += run.scenario_length;
        }

        EXPECT_EQ(profile.getAllRunRecords().size(), 360U);
        ASSERT_EQ(seconds_by_day.size(), 2U);
        const auto first = seconds_by_day.begin();
        const auto second = std::next(first);
        EXPECT_DOUBLE_EQ(first->second, 5.0 * 60.0 * 60.0);
        EXPECT_EQ(second->first - first->first, days{5});
        EXPECT_DOUBLE_EQ(second->second, 1.0 * 60.0 * 60.0);
    }

    TEST(GalleryDatasetTest, RichProfileUsesNormalLengthRunsWithCoherentTotals) {
        const auto profile = ksv::gallery::makeProfile(Dataset::RichProfile);

        for (const auto &run: profile.getAllRunRecords()) {
            EXPECT_GE(run.scenario_length, 30.0F);
            EXPECT_LE(run.scenario_length, 90.0F);
            EXPECT_EQ(run.stored_totals.shots, run.stored_totals.hits + run.stored_totals.misses);
            EXPECT_LE(run.stored_totals.kills, run.stored_totals.hits);
            EXPECT_GE(run.stored_totals.accuracy(), 0.5);
            EXPECT_LE(run.stored_totals.accuracy(), 0.95);
        }
    }
}
