#include <gtest/gtest.h>

#include <QDate>
#include <QDateTime>
#include <QTimeZone>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

#include "capture/capture_sources.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/graphs/common/axes/date_time_axis.h"
#include "presentation/graphs/completion_history_vm.h"
#include "presentation/graphs/graph_vm.h"
#include "presentation/graphs/playtime_graph_vm.h"

namespace ksv::ui {
    namespace {
        using namespace ksv::application;
        using presentation::BenchmarkHistoryViewModel;
        using presentation::CompletionHistoryViewModel;
        using presentation::DateTimeAxis;
        using presentation::GraphViewModel;
        using presentation::PlaytimeGraphViewModel;

        class FakeGraphUseCase final : public IGraphUseCase {
        public:
            std::string run_label;

            void load_perf(std::string_view) override {}
            void load_latest_perf() override {}
            std::string get_run_label() override { return run_label; }
            bool hasCurrentRun() const override { return false; }
            bool currentRunHasPerformance() const override { return false; }
            void onCurrentPerfChanged(std::function<void()>) override {}
            void onSeriesConfigChanged(std::function<void()>) override {}
            std::vector<SeriesId> getAxisLabelSeriesIds() override { return {}; }
            void onAxisLabelChanged(std::function<void()>) override {}
            [[nodiscard]] GraphPresentation presentation() const override {
                return defaultGraphPresentation(GraphKey::Run);
            }
            void onPresentationChanged(std::function<void()>) override {}
            std::vector<SeriesConfig> getSeriesConfigs() override { return {}; }
            std::optional<SeriesPoints> getSeriesValues(SeriesId) override { return std::nullopt; }
            std::vector<AxisConfig> getAxes() override { return {}; }
            double getRunDuration() override { return 0.0; }
        };

        class FakePlaytimeUseCase final : public IPlaytimeGraphUseCase {
        public:
            PlaytimeGraphSnapshot current;

            [[nodiscard]] PlaytimeGraphSnapshot snapshot() override { return current; }
            void onChanged(std::function<void()>) override {}
            [[nodiscard]] GraphPresentation presentation() const override {
                return defaultGraphPresentation(GraphKey::DailyPlaytime);
            }
            void onPresentationChanged(std::function<void()>) override {}
        };

        class FakeCompletionHistoryUseCase final : public ICompletionHistoryUseCase {
        public:
            CompletionHistory history;

            CompletionHistory get_history() override { return history; }
            void onCurrentScenarioChanged(std::function<void()>) override {}
            void onRangeChanged(std::function<void()>) override {}
            [[nodiscard]] GraphPresentation presentation() const override {
                return defaultGraphPresentation(GraphKey::ScenarioHistory);
            }
            void onPresentationChanged(std::function<void()>) override {}
            std::optional<int> selected_run_index() override { return std::nullopt; }
            void onSelectedRunChanged(std::function<void()>) override {}
            void select_run(int) override {}
        };

        QString utcDay(const qint64 ms) {
            return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc()).date().toString(QStringLiteral("yyyy-MM-dd"));
        }

        QString localDay(const qint64 ms) {
            return QDateTime::fromMSecsSinceEpoch(ms).date().toString(QStringLiteral("yyyy-MM-dd"));
        }

        qint64 utcMidnightOfEpochDay(const long long epochDay) {
            return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
        }
    }

    TEST(CaptureContext, RunGraphNamesTheRunAndElapsedSeconds) {
        const QString label = QStringLiteral("Tile Frenzy 2026-03-04 18:22");
        auto useCase = std::make_shared<FakeGraphUseCase>();
        useCase->run_label = label.toStdString();
        GraphViewModel vm{useCase};
        vm.fetchData();

        const GraphCaptureContext context = runCaptureContext(vm);

        EXPECT_EQ(context.title, label);
        EXPECT_TRUE(context.xTitle.contains(QStringLiteral("elapsed"), Qt::CaseInsensitive));
        EXPECT_TRUE(context.xTitle.contains(QStringLiteral("second"), Qt::CaseInsensitive));
        EXPECT_FALSE(context.graphType.isEmpty());
        EXPECT_NE(context.graphType, context.title);
        EXPECT_TRUE(context.rangeText.isEmpty());
    }

    TEST(CaptureContext, PlaytimeStatesTheSelectedIntervalPastTheLastObservation) {
        constexpr long long kFirstDay = 19495;
        constexpr long long kLastSampleDay = 19500;
        constexpr long long kIntervalEndDay = kLastSampleDay + 10;
        auto useCase = std::make_shared<FakePlaytimeUseCase>();
        useCase->current.view.interval = DayInterval{std::chrono::sys_days{std::chrono::days{kFirstDay}},
                                                     std::chrono::sys_days{std::chrono::days{kIntervalEndDay}}};
        useCase->current.samples.emplace_back(std::chrono::sys_days{std::chrono::days{kLastSampleDay}}, 1800.0);
        PlaytimeGraphViewModel vm{useCase};
        vm.refresh();

        const GraphCaptureContext context = dailyPlaytimeCaptureContext(vm);

        const QString lastSample = utcDay(utcMidnightOfEpochDay(kLastSampleDay));
        const QString intervalEnd = utcDay(utcMidnightOfEpochDay(kIntervalEndDay));
        EXPECT_TRUE(context.rangeText.contains(utcDay(utcMidnightOfEpochDay(kFirstDay))));
        EXPECT_TRUE(context.rangeText.contains(intervalEnd));
        EXPECT_GT(intervalEnd, lastSample);
        EXPECT_EQ(context.title, vm.title());
        EXPECT_FALSE(context.graphType.isEmpty());
    }

    TEST(CaptureContext, ScenarioHistoryCountsRunsAndDatesThem) {
        constexpr qint64 kFirstStartMs = 1'772'000'000'000;
        constexpr qint64 kLastStartMs = 1'774'000'000'000;
        auto useCase = std::make_shared<FakeCompletionHistoryUseCase>();
        useCase->history = {.scenario_name = "Air Angelic", .full_count = 10};
        for (const int run: {4, 5, 6})
            useCase->history.rows.push_back({.run_index = run, .start_time_ms = kFirstStartMs, .score = 100.0});
        useCase->history.rows.back().start_time_ms = kLastStartMs;
        CompletionHistoryViewModel vm{useCase};
        vm.refresh();

        const GraphCaptureContext context = scenarioHistoryCaptureContext(vm);

        EXPECT_EQ(context.title, vm.scenarioTitle());
        EXPECT_TRUE(context.rangeText.contains(QStringLiteral("#4")));
        EXPECT_TRUE(context.rangeText.contains(QStringLiteral("#6")));
        EXPECT_TRUE(context.rangeText.contains(localDay(kFirstStartMs)));
        EXPECT_TRUE(context.rangeText.contains(localDay(kLastStartMs)));
        EXPECT_TRUE(context.xTitle.contains(QStringLiteral("run"), Qt::CaseInsensitive));
        EXPECT_TRUE(context.graphType.contains(QStringLiteral("scenario"), Qt::CaseInsensitive));
        EXPECT_TRUE(context.graphType.contains(QStringLiteral("history"), Qt::CaseInsensitive));
    }

    TEST(CaptureContext, ScenarioHistoryNamesASingleRunAndDateOnce) {
        constexpr qint64 kStartMs = 1'772'000'000'000;
        auto useCase = std::make_shared<FakeCompletionHistoryUseCase>();
        useCase->history = {.scenario_name = "Air Angelic", .full_count = 10};
        useCase->history.rows.push_back({.run_index = 5, .start_time_ms = kStartMs, .score = 100.0});
        CompletionHistoryViewModel vm{useCase};
        vm.refresh();

        const GraphCaptureContext context = scenarioHistoryCaptureContext(vm);

        EXPECT_EQ(context.rangeText.count(QStringLiteral("#5")), 1);
        EXPECT_EQ(context.rangeText.count(localDay(kStartMs)), 1);
    }

    TEST(CaptureContext, ScenarioHistoryNamesADateSharedByTheEndsOnce) {
        constexpr qint64 kStartMs = 1'772'000'000'000;
        auto useCase = std::make_shared<FakeCompletionHistoryUseCase>();
        useCase->history = {.scenario_name = "Air Angelic", .full_count = 10};
        for (const int run: {4, 5})
            useCase->history.rows.push_back({.run_index = run, .start_time_ms = kStartMs, .score = 100.0});
        CompletionHistoryViewModel vm{useCase};
        vm.refresh();

        const GraphCaptureContext context = scenarioHistoryCaptureContext(vm);

        EXPECT_TRUE(context.rangeText.contains(QStringLiteral("#4")));
        EXPECT_TRUE(context.rangeText.contains(QStringLiteral("#5")));
        EXPECT_EQ(context.rangeText.count(localDay(kStartMs)), 1);
    }

    TEST(CaptureContext, BenchmarkHistoryCarriesTheBenchmarkAndMetric) {
        const qint64 firstMs = utcMidnightOfEpochDay(19480);
        const qint64 lastMs = utcMidnightOfEpochDay(19520);
        DateTimeAxis axis;
        ASSERT_TRUE(axis.setEpochMillisecondsRange(firstMs, lastMs));
        BenchmarkHistoryViewModel vm{BenchmarkHistoryViewModel::AverageRank, axis};

        const GraphCaptureContext context = benchmarkHistoryCaptureContext(QStringLiteral("Voltaic"), vm);

        EXPECT_EQ(context.title, QStringLiteral("Voltaic"));
        EXPECT_EQ(context.graphType, vm.metricName());
        EXPECT_TRUE(context.rangeText.contains(utcDay(firstMs)));
        EXPECT_TRUE(context.rangeText.contains(utcDay(lastMs)));
        EXPECT_EQ(context.xTitle, QStringLiteral("Date"));
    }
}
