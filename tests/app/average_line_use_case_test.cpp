#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "usecases/average_line_use_case.h"
#include "usecases/bucketed_run.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeProfileService final : public IProfileService {
    public:
        std::vector<ScenarioPerf> runs;

        void generateProfileFromDirectory() override {
        }

        void loadProfile() override {
        }

        void onBuildRequested(std::function<void()>) override {
        }

        void beginProfileBuild() override {
        }

        void applyBuiltProfile(UserProfile) override {
        }

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return {}; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &, std::size_t) const override {
            return {};
        }

        [[nodiscard]] std::vector<ScenarioPerf> getRunsForScenario(const ScenarioId &scenario) const override {
            std::vector<ScenarioPerf> result;
            for (const auto &run: runs) if (run.run_id.scenario_id == scenario) result.push_back(run);
            return result;
        }

        [[nodiscard]] std::vector<RunData> getCompletionHistory(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override {
            return {};
        }

        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &) const override { return {}; }
        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &) const override {
            return {};
        }

        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &) const override { return {}; }
        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(std::size_t) const override { return {}; }

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> > getRollingTimeAverage(int) const override {
            return {};
        }

        [[nodiscard]] bool isProfileLoaded() const override { return true; }

        void onProfileChanged(std::function<void()>) override {
        }
    };

    ScenarioPerf run(const long long start, std::initializer_list<double> scores, const std::string hash = "scenario") {
        ScenarioPerf perf;
        perf.run_id = {{"Scenario", hash}, start};
        float time = 1.0F;
        for (const auto score: scores) {
            perf.data.emplace_back(time);
            perf.data.back().score = static_cast<float>(score);
            ++time;
        }
        return perf;
    }

    AverageLineUseCase evaluator(const std::shared_ptr<FakeProfileService> &profiles) {
        return AverageLineUseCase{profiles};
    }

    TEST(BucketedRunTest, MatchesPerfColumnBuilderPrimitiveProjection) {
        ScenarioPerf perf;
        perf.data = {ScenarioDataPoint{0.1F}, ScenarioDataPoint{1.0F}};
        perf.data[0].score = 99.0F;
        perf.data[1].score = 10.0F;
        perf.data[1].shots = 2;
        perf.data[1].hits = 1;

        const auto buckets = bucketRun(perf);
        ASSERT_EQ(buckets.times, (std::vector<float>{1.0F}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Score), (std::vector<double>{10.0}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Shots), (std::vector<double>{2.0}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Hits), (std::vector<double>{1.0}));
    }

    TEST(AverageLineUseCaseTest, EvaluatesEveryExpressionVariantInDouble) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {2, 4});
        profiles->runs = {reference, run(200, {4, 8}), run(300, {6, 12})};
        const auto expression = averageAcrossRuns(
            projectRateToFinal(rollingMean(
                projectedFinalValue(runningSum(add(primitive(PrimitiveMetric::Score), numericConstant(1.0)))), 2)),
            RecentRuns{2});
        const auto result = evaluator(profiles).evaluate(reference, expression);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->size(), 2U);
        EXPECT_TRUE(std::isfinite(result->front()));
    }

    TEST(AverageLineUseCaseTest, RecentRunsExcludesReferenceFiltersThenSelectsNewest) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1});
        profiles->runs = {
            reference, run(200, {10}), run(300, {20}), run(400, {30}),
            run(500, {std::numeric_limits<double>::infinity()})
        };
        const auto result = evaluator(profiles).evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{2}));
        ASSERT_TRUE(result);
        EXPECT_DOUBLE_EQ(result->front(), 25.0);
    }

    TEST(AverageLineUseCaseTest, TopPercentileUsesFinalTotalScoreRoundsUpAndKeepsTies) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1});
        profiles->runs = {reference, run(200, {10}), run(300, {30}), run(400, {30}), run(500, {20})};
        const auto result = evaluator(profiles).evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), TopPercentile{25.0}));
        ASSERT_TRUE(result);
        EXPECT_DOUBLE_EQ(result->front(), 30.0);
    }

    TEST(AverageLineUseCaseTest, RequiresTwoCandidatesAndUsesReferenceLengthOnly) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 2});
        profiles->runs = {reference, run(200, {3, 4})};
        EXPECT_FALSE(
            evaluator(profiles).evaluate(reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{2})
            ));
        profiles->runs.push_back(run(300, {5, 6}));
        const auto result = evaluator(profiles).evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{2}));
        ASSERT_TRUE(result);
        EXPECT_EQ(result->size(), 2U);
    }

    TEST(AverageLineUseCaseTest, DiscardsNonFiniteCandidatesAndNeverClampsOrPads) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1});
        profiles->runs = {
            reference, run(200, {1000000}), run(300, {2000000}), run(400, {std::numeric_limits<double>::infinity()}),
            run(500, {2, 3})
        };
        const auto result = evaluator(profiles).evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{3}));
        ASSERT_TRUE(result);
        ASSERT_EQ(result->size(), 1U);
        EXPECT_DOUBLE_EQ(result->front(), 1500000.0);
    }
}
