#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "usecases/average_line_use_case.h"
#include "usecases/bucketed_run.h"
#include "fake_profile_service.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    ksv::domain::Run run(const long long start, std::initializer_list<double> scores, const std::string hash = "scenario") {
        ksv::domain::Run perf;
        perf.run_id = {{"Scenario", hash}, start};
        perf.performance.emplace();
        float time = 1.0F;
        for (const auto score: scores) {
            perf.performance->samples.emplace_back(time);
            perf.performance->samples.back().score = static_cast<float>(score);
            perf.stored_totals.score += static_cast<float>(score);
            ++time;
        }
        return perf;
    }

    ksv::domain::Run csvOnly(const long long start, const float score, const std::string hash = "scenario") {
        ksv::domain::Run csv;
        csv.run_id = {{"Scenario", hash}, start};
        csv.stored_totals.score = score;
        return csv;
    }

    std::optional<std::vector<double>> evaluate(const std::shared_ptr<FakeProfileService> &profiles,
                                                const ksv::domain::Run &reference, const Expression &expression) {
        return AverageLineUseCase{profiles}.evaluate(reference, expression);
    }

    const auto kScoreSum = [] { return runningSum(primitive(PrimitiveMetric::Score)); };

    TEST(BucketedRunTest, MatchesPerfColumnBuilderPrimitiveProjection) {
        ksv::domain::Run perf;
        perf.performance.emplace();
        perf.performance->samples = {ksv::domain::ScenarioDataPoint{0.1F}, ksv::domain::ScenarioDataPoint{1.0F}};
        perf.performance->samples[0].score = 99.0F;
        perf.performance->samples[1].score = 10.0F;
        perf.performance->samples[1].shots = 2;
        perf.performance->samples[1].hits = 1;

        const auto buckets = bucketRun(perf);
        ASSERT_EQ(buckets.times, (std::vector<float>{1.0F}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Score), (std::vector<double>{10.0}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Shots), (std::vector<double>{2.0}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Hits), (std::vector<double>{1.0}));
    }

    TEST(BucketedRunTest, IgnoresSamplesBeyondMaximumDuration) {
        ksv::domain::Run perf;
        perf.performance.emplace();
        perf.performance->samples = {ksv::domain::ScenarioDataPoint{1.0F}, ksv::domain::ScenarioDataPoint{3601.0F}};
        perf.performance->samples[0].score = 10.0F;
        perf.performance->samples[1].score = 99.0F;

        const auto buckets = bucketRun(perf);

        EXPECT_EQ(buckets.times, (std::vector<float>{1.0F}));
        EXPECT_EQ(buckets.valuesFor(PrimitiveMetric::Score), (std::vector<double>{10.0}));
    }

    TEST(AverageLineUseCaseTest, EvaluatesEveryExpressionVariantInDouble) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {2, 4});
        profiles->runs = {reference, run(200, {4, 8}), run(300, {6, 12})};
        const auto expression = averageAcrossRuns(
            projectRateToFinal(rollingMean(
                projectedFinalValue(runningSum(add(primitive(PrimitiveMetric::Score), numericConstant(1.0)))), 2)),
            RecentRuns{2});
        const auto result = AverageLineUseCase{profiles}.evaluate(reference, expression);
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
        const auto result = AverageLineUseCase{profiles}.evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{2}));
        ASSERT_TRUE(result);
        EXPECT_DOUBLE_EQ(result->front(), 25.0);
    }

    TEST(AverageLineUseCaseTest, TopPercentileUsesFinalTotalScoreRoundsUpAndKeepsTies) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1});
        profiles->runs = {reference, run(200, {10}), run(300, {30}), run(400, {30}), run(500, {20})};
        const auto result = AverageLineUseCase{profiles}.evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), TopPercentile{25.0}));
        ASSERT_TRUE(result);
        EXPECT_DOUBLE_EQ(result->front(), 30.0);
    }

    TEST(AverageLineUseCaseTest, RequiresTwoCandidatesAndUsesReferenceLengthOnly) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 2});
        profiles->runs = {reference, run(200, {3, 4})};
        EXPECT_FALSE(
            AverageLineUseCase{profiles}.evaluate(reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{2})
            ));
        profiles->runs.push_back(run(300, {5, 6}));
        const auto result = AverageLineUseCase{profiles}.evaluate(
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
        const auto result = AverageLineUseCase{profiles}.evaluate(
            reference, averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{3}));
        ASSERT_TRUE(result);
        ASSERT_EQ(result->size(), 1U);
        EXPECT_DOUBLE_EQ(result->front(), 1500000.0);
    }

    TEST(AverageLineUseCaseTest, FinalValueIsAFlatLineOfTheStoredTotalAtTheReferenceLength) {
        auto profiles = std::make_shared<FakeProfileService>();
        auto reference = run(100, {2, 4, 6});
        reference.stored_totals = {.score = 153.5F, .shots = 180, .hits = 166, .misses = 14, .kills = 150};
        profiles->runs = {reference};
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Score)),
                  (std::vector<double>{153.5, 153.5, 153.5}));
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Shots)), (std::vector<double>(3, 180.0)));
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Hits)), (std::vector<double>(3, 166.0)));
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Misses)), (std::vector<double>(3, 14.0)));
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Kills)), (std::vector<double>(3, 150.0)));
    }

    TEST(AverageLineUseCaseTest, FinalDamageIsAbsentWithoutAStoredDamageTotal) {
        auto profiles = std::make_shared<FakeProfileService>();
        auto reference = run(100, {1, 2});
        profiles->runs = {reference};
        EXPECT_FALSE(evaluate(profiles, reference, finalValue(FinalStat::Dmg)));
        reference.stored_totals.damage = 166.0F;
        EXPECT_EQ(evaluate(profiles, reference, finalValue(FinalStat::Dmg)), (std::vector<double>{166.0, 166.0}));
    }

    TEST(AverageLineUseCaseTest, BestRunPicksHighestScoreAndExcludesTheCurrentRun) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {50, 50});
        profiles->runs = {reference, run(200, {10, 10}), run(300, {30, 40}), run(400, {5, 5})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(kScoreSum())), (std::vector<double>{30, 70}));
    }

    TEST(AverageLineUseCaseTest, BestRunBreaksScoreTiesTowardTheEarliestRun) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference, run(400, {5, 5}), run(200, {2, 8}), run(300, {9, 1})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(primitive(PrimitiveMetric::Score))),
                  (std::vector<double>{2, 8}));
    }

    TEST(AverageLineUseCaseTest, BestRunKeepsItsOwnLengthWhateverTheReferenceLength) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference, run(200, {5, 5, 5, 5})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(primitive(PrimitiveMetric::Score)))->size(), 4U);
        profiles->runs = {reference, run(200, {5})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(primitive(PrimitiveMetric::Score)))->size(), 1U);
    }

    TEST(AverageLineUseCaseTest, BestRunOfFinalSpansTheReferenceEvenWhenTheBestRunHasNoPerf) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1, 1});
        profiles->runs = {reference, csvOnly(200, 900.0F), run(300, {5})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(finalValue(FinalStat::Score))),
                  (std::vector<double>(3, 900.0)));
    }

    TEST(AverageLineUseCaseTest, BestRunOfPrimitiveBackedExpressionDoesNotFallBackWhenTheBestRunHasNoPerf) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1, 1});
        profiles->runs = {reference, csvOnly(200, 900.0F), run(300, {5, 5, 5})};
        const auto result = evaluate(profiles, reference, bestRun(kScoreSum()));
        EXPECT_TRUE(!result || result->empty());
    }

    TEST(AverageLineUseCaseTest, BestRunIsAbsentWhenOnlyTheCurrentRunExists) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference};
        EXPECT_FALSE(evaluate(profiles, reference, bestRun(finalValue(FinalStat::Score))));
    }

    TEST(AverageLineUseCaseTest, NestedBestRunMeansSecondBestBecauseTheInnerOneExcludesTheOuterBestRun) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference, run(200, {10, 10}), run(300, {30, 30}), run(400, {20, 20})};
        EXPECT_EQ(evaluate(profiles, reference, bestRun(bestRun(primitive(PrimitiveMetric::Score)))),
                  (std::vector<double>{20, 20}));
    }

    TEST(AverageLineUseCaseTest, SubtractingBestRunPaceFromMismatchedLengthsIsAbsent) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference, run(200, {5, 5, 5})};
        EXPECT_FALSE(evaluate(profiles, reference, subtract(kScoreSum(), bestRun(kScoreSum()))));
        profiles->runs = {reference, run(200, {5, 5})};
        EXPECT_EQ(evaluate(profiles, reference, subtract(kScoreSum(), bestRun(kScoreSum()))),
                  (std::vector<double>{-4, -8}));
    }

    TEST(AverageLineUseCaseTest, PurelyConstantAverageInputAdmitsCandidatesOfAnyLength) {
        auto profiles = std::make_shared<FakeProfileService>();
        const auto reference = run(100, {1, 1});
        profiles->runs = {reference, run(200, {1}), run(300, {1, 2, 3})};
        const auto result = evaluate(profiles, reference, averageAcrossRuns(numericConstant(3.0), RecentRuns{5}));
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, (std::vector<double>{3.0, 3.0}));
    }
}
