//
// PerfColumnBuilder tests: whole-second resampling and per-column derivation
// from raw ScenarioPerf data.
//

#include <gtest/gtest.h>

#include "usecases/perf_column_builder.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    ScenarioDataPoint make_point(const float time, const int shots, const int hits, const float score,
                                  const int kills = 0, const float dmg = 0.0F) {
        auto point = ScenarioDataPoint(time);
        point.time = time;
        point.shots = shots;
        point.hits = hits;
        point.score = score;
        point.kills = kills;
        point.dmg = dmg;
        return point;
    }

    ScenarioPerf make_perf(std::vector<ScenarioDataPoint> points, const float scenario_length = 0.0F) {
        ScenarioPerf perf;
        perf.data = std::move(points);
        perf.scenario_length = scenario_length;
        return perf;
    }

    TEST(PerfColumnBuilderTest, EmptyPerfProducesEmptySeries) {
        const auto series = PerfColumnBuilder::build(ScenarioPerf{});

        EXPECT_TRUE(series.times.empty());
        EXPECT_TRUE(series.columns.empty());
    }

    TEST(PerfColumnBuilderTest, RoundsTimestampsToNearestWholeSecond) {
        const auto perf = make_perf({make_point(0.3F, 10, 5, 10.0F), make_point(0.6F, 10, 5, 20.0F)});

        const auto series = PerfColumnBuilder::build(perf);

        ASSERT_EQ(series.times.size(), 2);
        EXPECT_EQ(series.times[0], 0.0F);
        EXPECT_EQ(series.times[1], 1.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Score)[0], 10.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Score)[1], 20.0F);
    }

    TEST(PerfColumnBuilderTest, FillsSecondsWithNoRawDataAsZero) {
        const auto perf = make_perf({make_point(0.1F, 2, 1, 10.0F), make_point(3.4F, 4, 2, 20.0F)});

        const auto series = PerfColumnBuilder::build(perf);

        ASSERT_EQ(series.times.size(), 4);
        const auto &score = series.columns.at(ColumnId::Score);
        const auto &shots = series.columns.at(ColumnId::Shots);
        const auto &accuracy = series.columns.at(ColumnId::Accuracy);
        for (int second: {1, 2}) {
            EXPECT_EQ(score[second], 0.0F);
            EXPECT_EQ(shots[second], 0.0F);
            EXPECT_EQ(accuracy[second], 0.0F);
        }
        EXPECT_EQ(score[3], 20.0F);
    }

    // Near the end of a run, two ticks can land ~0.02s apart (e.g. x.87/x.89)
    // instead of the usual ~1s spacing. Rounded independently, the second
    // tick's small delta would look like an anomalous dip/spike right at the
    // end of the chart; summed into the same bucket as its neighbor, it
    // correctly contributes to that second's total instead.
    TEST(PerfColumnBuilderTest, SumsAdditiveColumnsWhenPointsRoundIntoTheSameSecond) {
        const auto perf = make_perf({
            make_point(58.87F, 3, 1, 1.0F, 0, 10.0F), make_point(58.89F, 4, 2, 2.0F, 1, 20.0F)
        });

        const auto series = PerfColumnBuilder::build(perf);

        const auto lastRow = series.times.size() - 1;
        EXPECT_EQ(series.times[lastRow], 59.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Score)[lastRow], 3.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Shots)[lastRow], 7.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Kills)[lastRow], 1.0F);
        EXPECT_EQ(series.columns.at(ColumnId::Dmg)[lastRow], 30.0F);
    }

    // Accuracy is a ratio, so merging two points that round into the same
    // second must recompute it from summed hits/shots rather than summing or
    // averaging the ratios themselves.
    TEST(PerfColumnBuilderTest, RecomputesAccuracyFromSummedHitsAndShotsWhenMerging) {
        const auto perf = make_perf({make_point(10.0F, 10, 5, 0.0F), make_point(10.1F, 90, 81, 0.0F)});

        const auto series = PerfColumnBuilder::build(perf);

        EXPECT_NEAR(series.columns.at(ColumnId::Accuracy)[10], 0.86, 1e-6);
    }

    TEST(PerfColumnBuilderTest, MergedBucketWithZeroShotsHasZeroAccuracy) {
        const auto perf = make_perf({make_point(20.0F, 0, 0, 0.0F), make_point(20.2F, 0, 0, 0.0F)});

        const auto series = PerfColumnBuilder::build(perf);

        EXPECT_EQ(series.columns.at(ColumnId::Accuracy)[20], 0.0F);
    }

    TEST(PerfColumnBuilderTest, ScoreTotalIsARunningSumOfPerSecondScore) {
        const auto perf = make_perf({
            make_point(0.0F, 1, 1, 10.0F), make_point(1.0F, 1, 1, 20.0F), make_point(2.0F, 1, 1, 5.0F)
        });

        const auto series = PerfColumnBuilder::build(perf);

        const auto &total = series.columns.at(ColumnId::ScoreTotal);
        ASSERT_EQ(total.size(), 3);
        EXPECT_EQ(total[0], 10.0F);
        EXPECT_EQ(total[1], 30.0F);
        EXPECT_EQ(total[2], 35.0F);
    }

    // Steady 10/s pace over a 2-bucket run: the average-pace-so-far projection
    // should read the same steady final value at every point, and that final
    // value must equal ScoreTotal at the last bucket.
    TEST(PerfColumnBuilderTest, ExpectedFinalScoreProjectsSteadyPaceAcrossFullDuration) {
        const auto perf = make_perf({make_point(0.0F, 1, 1, 10.0F), make_point(1.0F, 1, 1, 10.0F)});

        const auto series = PerfColumnBuilder::build(perf);

        const auto &expected = series.columns.at(ColumnId::ExpectedFinalScore);
        const auto &total = series.columns.at(ColumnId::ScoreTotal);
        ASSERT_EQ(expected.size(), 2);
        EXPECT_NEAR(expected[0], 20.0F, 1e-4);
        EXPECT_NEAR(expected[1], 20.0F, 1e-4);
        EXPECT_NEAR(expected[1], total[1], 1e-4);
    }

    // scenario_length reflects scaled time on scenarios that manipulate time
    // flow, so it can diverge from the run's real observed duration; the
    // projection must extrapolate against the latter, converging exactly to
    // ScoreTotal at the final bucket regardless of scenario_length.
    TEST(PerfColumnBuilderTest, ExpectedFinalScoreIgnoresScenarioLengthAndConvergesToScoreTotal) {
        const auto perf = make_perf({
            make_point(0.0F, 1, 1, 10.0F), make_point(1.0F, 1, 1, 10.0F), make_point(2.0F, 1, 1, 10.0F)
        }, 10.0F);

        const auto series = PerfColumnBuilder::build(perf);

        const auto &expected = series.columns.at(ColumnId::ExpectedFinalScore);
        const auto &total = series.columns.at(ColumnId::ScoreTotal);
        ASSERT_EQ(expected.size(), 3);
        EXPECT_NEAR(expected[2], total[2], 1e-4);
        EXPECT_NEAR(expected[2], 30.0F, 1e-4);
    }

    // An outlier-heavy first second (100) dragged into an otherwise steady
    // 10/s pace inflates the whole-run average projection, but the
    // trailing-5s projection ignores it once it ages out of the window -
    // that's the entire reason it exists as a separate column.
    TEST(PerfColumnBuilderTest, ExpectedFinalScoreRecentIgnoresPaceOlderThanTheTrailingWindow) {
        const auto perf = make_perf({
            make_point(0.0F, 1, 1, 100.0F), make_point(1.0F, 1, 1, 10.0F), make_point(2.0F, 1, 1, 10.0F),
            make_point(3.0F, 1, 1, 10.0F), make_point(4.0F, 1, 1, 10.0F), make_point(5.0F, 1, 1, 10.0F)
        }, 6.0F);

        const auto series = PerfColumnBuilder::build(perf);

        const auto &overall = series.columns.at(ColumnId::ExpectedFinalScore);
        const auto &recent = series.columns.at(ColumnId::ExpectedFinalScoreRecent);
        ASSERT_EQ(recent.size(), 6);

        // Overall average (150 total / 6s) still carries the outlier.
        EXPECT_NEAR(overall[5], 150.0F, 1e-4);
        // Recent pace (last 5s: 10/s, the outlier at t=0 has aged out) projects lower.
        EXPECT_NEAR(recent[5], 60.0F, 1e-4);
    }
}
