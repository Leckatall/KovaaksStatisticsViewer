//
// QUAL-03 / QUAL-04 over a large generated profile (tools/driver specs, the gallery run generator):
// which committed changes recompute full history, how long each user-visible change takes on the
// real wired App, and that selection, drafts and benchmark summaries ride out configuration changes.
// Timings are measurements, printed and recorded, never asserted.
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/app.h"
#include "driver_spec.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_tracking_vm.h"
#include "presentation/completion_history_vm.h"
#include "presentation/graph_vm.h"
#include "presentation/playtime_graph_vm.h"
#include "presentation/scenario_browser_vm.h"
#include "presentation/settings_vm.h"
#include "usecases/benchmark_tracking_use_case.h"
#include "usecases/completion_history_use_case.h"
#include "usecases/playtime_graph_use_case.h"

#include "fake_graph_configuration_persistence.h"
#include "integration_env.h"

using namespace ksv;
using namespace ksv::application;

namespace {
    constexpr int kScenarios = 200;
    constexpr int kHeavyRuns = 1500;
    constexpr int kOtherRuns = 90;
    constexpr int kBenchmarkScenarios = 40;
    constexpr int kProfileDays = 1000;
    constexpr auto kHeavyHash = "heavy-hash";
    constexpr auto kBenchmarkId = "perf-benchmark";

    QJsonObject profileSpec() {
        QJsonArray scenarios;
        QJsonArray benchmarked;
        const auto add = [&](const QString &name, const QJsonObject &fields) {
            QJsonObject scenario{{"name", name}};
            for (auto it = fields.begin(); it != fields.end(); ++it) scenario.insert(it.key(), it.value());
            scenarios.append(scenario);
            if (benchmarked.size() < kBenchmarkScenarios) benchmarked.append(name);
        };
        // The scenario every history graph is pointed at: three runs a day, every other day.
        add("Heavy Scenario", {{"hash", kHeavyHash}, {"runs", kHeavyRuns}, {"runsPerDay", 3}, {"daySpacing", 2}});
        for (int i = 1; i < kScenarios; ++i) {
            add(QString("Scenario %1").arg(i, 3, 10, QChar('0')),
                {{"hash", QString("hash-%1").arg(i, 3, 10, QChar('0'))}, {"runs", kOtherRuns}, {"firstDay", (i * 7) % 22},
                 {"daySpacing", 22}, {"runsPerDay", 2}, {"durationSeconds", 30}});
        }
        return {
            {"scenarios", scenarios},
            {
                "benchmark", QJsonObject{
                    {"id", kBenchmarkId}, {"name", "Performance Benchmark"},
                    {"categories", QJsonArray{QJsonObject{{"name", "All"}, {"scenarios", benchmarked}}}}
                }
            }
        };
    }

    // The app's profile service with call counters, handed to use cases built beside the app's own.
    class CountingProfile final : public IProfileService {
    public:
        explicit CountingProfile(std::shared_ptr<IProfileService> inner) : m_inner(std::move(inner)) {
        }

        struct Calls {
            int rollingByWindow = 0;
            int rollingForScenarios = 0;
            int runFacts = 0;
            int completionHistory = 0;
            int runsForScenario = 0;

            int total() const { return rollingByWindow + rollingForScenarios + runFacts + completionHistory + runsForScenario; }
        };

        // Counted from const methods.
        mutable Calls calls;

        void generateProfileFromDirectory() override { m_inner->generateProfileFromDirectory(); }
        ProfileImportRequest beginImport(ProfileImportMode mode, ProfileImportReason reason) override {
            return m_inner->beginImport(mode, reason);
        }
        ProfileImportOutcome acceptImport(ProfileImportResult &result) override { return m_inner->acceptImport(result); }
        void abandonImport(ProfileImportToken token) override { m_inner->abandonImport(token); }
        void onRefreshRequested(std::function<void(ProfileImportReason)> callback) override {
            m_inner->onRefreshRequested(std::move(callback));
        }
        void loadProfile() override { m_inner->loadProfile(); }
        void onBuildRequested(std::function<void()> callback) override { m_inner->onBuildRequested(std::move(callback)); }
        std::vector<domain::ScenarioId> getScenarioList() const override { return m_inner->getScenarioList(); }
        domain::Run getPerf(const std::string &path) const override { return m_inner->getPerf(path); }
        domain::Run getLatestRun() const override { return m_inner->getLatestRun(); }

        std::optional<domain::Run> getMostRecentRun(const domain::ScenarioId &scenario) const override {
            return m_inner->getMostRecentRun(scenario);
        }

        std::vector<domain::Run> getMostRecentRuns(const domain::ScenarioId &scenario, std::size_t count) const override {
            return m_inner->getMostRecentRuns(scenario, count);
        }

        std::vector<domain::Run> getRunsForScenario(const domain::ScenarioId &scenario) const override {
            ++calls.runsForScenario;
            return m_inner->getRunsForScenario(scenario);
        }

        std::vector<domain::RunSummary> getCompletionHistory(const domain::ScenarioId &scenario) const override {
            ++calls.completionHistory;
            return m_inner->getCompletionHistory(scenario);
        }

        std::optional<float> getAverageScore(const domain::ScenarioId &scenario, std::size_t count) const override {
            return m_inner->getAverageScore(scenario, count);
        }

        std::optional<domain::Run> getCurrentRun(const domain::ScenarioRunId &run_id) const override {
            return m_inner->getCurrentRun(run_id);
        }

        std::optional<std::size_t> getRunCount(const domain::ScenarioId &scenario) const override {
            return m_inner->getRunCount(scenario);
        }

        std::optional<std::chrono::sys_seconds> getLastRunTime(const domain::ScenarioId &scenario) const override {
            return m_inner->getLastRunTime(scenario);
        }

        std::optional<double> getTotalTime(const domain::ScenarioId &scenario) const override {
            return m_inner->getTotalTime(scenario);
        }

        std::vector<domain::Run> getRecentRuns(std::size_t count) const override { return m_inner->getRecentRuns(count); }

        std::vector<std::pair<std::chrono::sys_days, double> > getRollingTimeAverage(int window_days) const override {
            ++calls.rollingByWindow;
            return m_inner->getRollingTimeAverage(window_days);
        }

        std::vector<domain::RunFact> getRunFacts(const std::vector<domain::ScenarioId> &scenarios) const override {
            ++calls.runFacts;
            return m_inner->getRunFacts(scenarios);
        }

        std::vector<std::pair<std::chrono::sys_days, double> > getRollingTimeAverage(
            const std::vector<domain::ScenarioId> &scenarios, int window_days) const override {
            ++calls.rollingForScenarios;
            return m_inner->getRollingTimeAverage(scenarios, window_days);
        }

        bool isProfileLoaded() const override { return m_inner->isProfileLoaded(); }
        void onProfileChanged(std::function<void()> callback) override { m_inner->onProfileChanged(std::move(callback)); }

    private:
        std::shared_ptr<IProfileService> m_inner;
    };

    struct Timing {
        std::string name;
        double coldMs;
        double warmMedianMs;
        double warmMaxMs;
        int samples;
    };

    // The real App over a generated profile in its store, a benchmark on disk, and "today" at the
    // profile's last day. Everything is built per test, so tests never share committed state.
    class LargeProfileTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        double loadMs = 0;
        int runs = 0;

        [[nodiscard]] virtual std::shared_ptr<data::IGraphConfigurationPersistence> persistence() {
            return env.graphConfigurationPersistence;
        }

        void SetUp() override {
            integration::clearSavedGraphConfiguration();
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_TRUE(QDir().mkpath(QFileInfo(env.profileStorePath()).absolutePath()));
            const QString specPath = QDir(env.rootPath()).filePath("profile-spec.json");
            {
                QFile out(specPath);
                ASSERT_TRUE(out.open(QIODevice::WriteOnly));
                out.write(QJsonDocument(profileSpec()).toJson());
            }
            const QString error = driver::writeProfileSpec(specPath, env.profileStorePath(), env.benchmarksDir.path());
            ASSERT_TRUE(error.isEmpty()) << error.toStdString();
            env.daySource->setToday(std::chrono::sys_days{std::chrono::year{2026} / std::chrono::August / 1} +
                                    std::chrono::days{kProfileDays});

            QElapsedTimer timer;
            timer.start();
            app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>(),
                                                     persistence(), env.statsParser, env.benchmarkStore,
                                                     env.daySource);
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 120000));
            loadMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;

            app->scenarioBrowserVm()->activateScenario(kHeavyHash, "Heavy Scenario");
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->completionHistoryVm()->runCount() == kHeavyRuns; }, 10000));
            app->benchmarkTrackingVm()->selectBenchmark(kBenchmarkId);
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->benchmarkTrackingVm()->state() == "trackable"; }, 30000))
                << app->benchmarkTrackingVm()->state().toStdString();
            for (const auto &scenario: app->profileService()->getScenarioList())
                runs += static_cast<int>(app->profileService()->getRunCount(scenario).value_or(0));
        }

        void TearDown() override {
            app.reset();
            integration::clearSavedGraphConfiguration();
        }

        [[nodiscard]] bool quick(const char *graph, const QVariantMap &patch) const {
            return app->settingsVm()->applyQuickPatch(graph, patch)["accepted"].toBool();
        }

        // The shipped ranges follow the latest run; rollover only moves ranges that follow today.
        [[nodiscard]] bool followToday() const {
            auto candidate = *app->graphConfigurationUseCase()->current().configuration;
            for (const auto key: {application::GraphKey::DailyPlaytime, application::GraphKey::BenchmarkRank,
                                  application::GraphKey::BenchmarkPlaytime})
                candidate.preferences(key).range->anchorToLatestRun = false;
            return app->graphConfigurationUseCase()->save(candidate).accepted();
        }

        void recordTimings(const char *storage);

        [[nodiscard]] static double timedMs(const std::function<void()> &action) {
            QElapsedTimer timer;
            timer.start();
            action();
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            return static_cast<double>(timer.nsecsElapsed()) / 1e6;
        }

        // `action(i)` must make a real change on every call; the first call is reported apart as cold.
        [[nodiscard]] static Timing measure(const std::string &name, const int samples,
                                            const std::function<void(int)> &action) {
            std::vector<double> times;
            for (int i = 0; i < samples; ++i) times.push_back(timedMs([&] { action(i); }));
            std::vector<double> warm(times.begin() + 1, times.end());
            std::ranges::sort(warm);
            return {name, times.front(), warm.empty() ? times.front() : warm[warm.size() / 2],
                    warm.empty() ? times.front() : warm.back(), samples};
        }
    };

    QStringList benchmarkSummary(const domain::BenchmarkProjection &projection) {
        QStringList lines;
        lines << QString("play=%1|avg=%2|history=%3|rank=%4").arg(projection.totalPlaytimeSeconds)
                .arg(projection.averageRank.value_or(-1.0)).arg(projection.averageRankHistory.size())
                .arg(projection.attainedRank ? projection.attainedRank->value.c_str() : "-");
        for (const auto &scenario: projection.scenarios)
            lines << QString("%1|runs=%2|pb=%3|norm=%4|play=%5").arg(scenario.name.c_str()).arg(scenario.runCount)
                    .arg(scenario.personalBest.value_or(-1.0)).arg(scenario.normalizedRank).arg(scenario.playtimeSeconds);
        return lines;
    }

    // ---- QUAL-04: which changes recompute what -----------------------------------------------

    TEST_F(LargeProfileTest, RangeStyleAndRolloverChangesNeverRecomputeFullHistory) {
        ASSERT_TRUE(followToday());
        auto config = app->graphConfigurationUseCase();
        auto day = app->daySource();
        CountingProfile counting(app->profileService());
        const auto profile = std::shared_ptr<IProfileService>(&counting, [](IProfileService *) {});
        // Beside the app's own use cases and fed by the same configuration, day source and profile data,
        // so each one sees exactly what the wired graph's would, behind a counting profile.
        auto playtime = std::make_unique<PlaytimeGraphUseCase>(profile, config, day);
        auto tracking = std::make_unique<BenchmarkTrackingUseCase>(app->benchmarksService(),
                                                                   app->benchmarkResolutionUseCase(), profile, config, day);
        auto history = std::make_unique<CompletionHistoryUseCase>(app->sessionController(), profile, config, day);
        int playtimeChanged = 0, trackingChanged = 0, trackingPlaytime = 0, trackingRank = 0, trackingStyle = 0;
        int historyRange = 0, historyStyle = 0, playtimeStyle = 0;
        playtime->onChanged([&] { ++playtimeChanged; });
        playtime->onPresentationChanged([&] { ++playtimeStyle; });
        tracking->onChanged([&] { ++trackingChanged; });
        tracking->onPlaytimeChanged([&] { ++trackingPlaytime; });
        tracking->onRankChanged([&] { ++trackingRank; });
        tracking->onPresentationChanged([&] { ++trackingStyle; });
        history->onRangeChanged([&] { ++historyRange; });
        history->onPresentationChanged([&] { ++historyStyle; });

        tracking->select(domain::BenchmarkId{kBenchmarkId});
        ASSERT_EQ(tracking->state(), BenchmarkTrackingState::Ready);
        (void) playtime->snapshot();
        (void) history->get_history();
        const auto resetCounters = [&] {
            counting.calls = {};
            playtimeChanged = trackingChanged = trackingPlaytime = trackingRank = trackingStyle = 0;
            historyRange = historyStyle = playtimeStyle = 0;
        };
        const auto readEverything = [&] {
            (void) playtime->snapshot();
            (void) tracking->snapshot();
            (void) history->get_history();
        };
        const std::array graphs{"run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"};

        // Style: nothing is notified as data, and no history is read.
        resetCounters();
        for (const auto *graph: graphs) ASSERT_TRUE(quick(graph, {{"connection", "straight"}, {"markers", true}}));
        readEverything();
        EXPECT_EQ(counting.calls.total() - counting.calls.completionHistory, 0);
        EXPECT_EQ(counting.calls.completionHistory, 1) << "only the test's own read of the visible rows";
        EXPECT_EQ(playtimeChanged + trackingChanged + trackingPlaytime + trackingRank + historyRange, 0);
        EXPECT_EQ(playtimeStyle, 1);
        EXPECT_EQ(trackingStyle, 2);
        EXPECT_EQ(historyStyle, 1);

        // Range: windowing happens on data already held.
        resetCounters();
        ASSERT_TRUE(quick("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 14}}));
        ASSERT_TRUE(quick("benchmarkPlaytime", {{"rangeKind", "recentDays"}, {"count", 14}}));
        ASSERT_TRUE(quick("benchmarkRank", {{"rangeKind", "recentDays"}, {"count", 30}}));
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 50}}));
        readEverything();
        EXPECT_EQ(counting.calls.rollingByWindow, 0) << "a daily-playtime range change re-read the full series";
        EXPECT_EQ(counting.calls.rollingForScenarios, 0) << "a benchmark-playtime range change re-read the full series";
        EXPECT_EQ(counting.calls.runFacts, 0) << "a benchmark range change re-evaluated the benchmark";
        EXPECT_EQ(counting.calls.completionHistory, 1) << "one history read for the one visible-rows request";
        EXPECT_EQ(playtimeChanged, 1);
        EXPECT_EQ(trackingPlaytime, 1);
        EXPECT_EQ(trackingRank, 1);
        EXPECT_EQ(trackingChanged, 0) << "ranges never publish a new benchmark evaluation";
        EXPECT_EQ(historyRange, 1);

        // Rollover under today-anchored ranges only moves the visible window.
        resetCounters();
        env.daySource->advance(1);
        readEverything();
        EXPECT_EQ(counting.calls.rollingByWindow, 0);
        EXPECT_EQ(counting.calls.rollingForScenarios, 0);
        EXPECT_EQ(counting.calls.runFacts, 0);
        EXPECT_GE(playtimeChanged + trackingPlaytime + trackingRank + historyRange, 3);

        // Window: the series for the new window is built once; evaluation and summaries are not redone.
        resetCounters();
        ASSERT_TRUE(quick("dailyPlaytime", {{"window", 14}}));
        readEverything();
        EXPECT_EQ(counting.calls.rollingByWindow, 1);
        EXPECT_EQ(counting.calls.rollingForScenarios, 0);
        EXPECT_EQ(counting.calls.runFacts, 0);
        EXPECT_EQ(counting.calls.completionHistory, 1);
        readEverything();
        EXPECT_EQ(counting.calls.rollingByWindow, 1) << "reading again must hit the cache";

        resetCounters();
        ASSERT_TRUE(quick("benchmarkPlaytime", {{"window", 14}}));
        readEverything();
        EXPECT_EQ(counting.calls.rollingForScenarios, 1);
        EXPECT_EQ(counting.calls.runFacts, 0) << "a benchmark playtime window must not redo the rank evaluation";
        EXPECT_EQ(counting.calls.rollingByWindow, 0);
        EXPECT_EQ(trackingChanged, 0);
        EXPECT_EQ(trackingRank, 0);

        // Editing in a settings visit recomputes nothing until Save, and Save only what changed.
        resetCounters();
        auto *vm = app->settingsVm();
        QSignalSpy runData(app->graphVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy playtimeData(app->playtimeVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy historyData(app->completionHistoryVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy rankData(app->benchmarkTrackingVm()->rankHistory(), &presentation::GraphViewModelBase::dataUpdated);
        vm->beginVisit();
        for (const auto *graph: graphs) vm->setSelectedGraph(graph);
        ASSERT_TRUE(vm->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "25"));
        ASSERT_TRUE(vm->setGraphConnection("dailyPlaytime", "pointsOnly"));
        EXPECT_EQ(runData.count() + playtimeData.count() + historyData.count() + rankData.count(), 0);
        readEverything();
        EXPECT_EQ(counting.calls.total() - counting.calls.completionHistory, 0);
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        EXPECT_EQ(playtimeData.count(), 0) << "a style-only change to daily playtime redrew its data";
        EXPECT_EQ(historyData.count(), 1);
        EXPECT_EQ(runData.count(), 0);
        EXPECT_EQ(rankData.count(), 0);
        vm->endVisit();

        // The shadows subscribed to long-lived services; drop them while their owners still exist.
        playtime.reset();
        tracking.reset();
        history.reset();
    }

    // ---- QUAL-04: how long each user-visible change takes ------------------------------------

    void LargeProfileTest::recordTimings(const char *storage) {
        std::vector<Timing> timings;
        const auto record = [&](Timing timing) { timings.push_back(std::move(timing)); };
        constexpr int kSamples = 6;
        const auto toggleRange = [&](const char *graph) {
            return [this, graph](const int i) {
                ASSERT_TRUE(quick(graph, i % 2 == 0 ? QVariantMap{{"rangeKind", "allHistory"}}
                                                     : QVariantMap{{"rangeKind", "recentDays"}, {"count", 30}}));
            };
        };
        const auto toggleStyle = [&](const char *graph) {
            return [this, graph](const int i) {
                ASSERT_TRUE(quick(graph, {{"connection", i % 2 == 0 ? "straight" : "smooth"}, {"markers", i % 2 == 0}}));
            };
        };
        const std::array<const char *, 5> graphs{"run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"};

        for (const auto *graph: {"dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"})
            record(measure(std::string("range change (recent 30 days <-> all history): ") + graph, kSamples, toggleRange(graph)));
        record(measure("range change (latest 100 runs <-> all runs): scenarioHistory", kSamples, [&](const int i) {
            ASSERT_TRUE(quick("scenarioHistory", i % 2 == 0 ? QVariantMap{{"rangeKind", "allHistory"}}
                                                           : QVariantMap{{"rangeKind", "latestRuns"}, {"count", 100}}));
        }));
        for (const auto *graph: graphs)
            record(measure(std::string("style change (straight+markers <-> smooth): ") + graph, kSamples, toggleStyle(graph)));

        constexpr std::array windows{5, 14, 30, 60, 90, 180};
        for (const auto *graph: {"dailyPlaytime", "benchmarkPlaytime"})
            record(measure(std::string("window change (a new window each time): ") + graph, kSamples, [&](const int i) {
                ASSERT_TRUE(quick(graph, {{"window", windows[static_cast<std::size_t>(i)]}}));
            }));
        record(measure("window change (back to a window already used): dailyPlaytime", kSamples, [&](const int i) {
            ASSERT_TRUE(quick("dailyPlaytime", {{"window", i % 2 == 0 ? 7 : 14}}));
        }));

        // Rollover under today-anchored ranges (three graphs follow today).
        ASSERT_TRUE(followToday());
        ASSERT_TRUE(quick("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 30}}));
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "recentDays"}, {"count", 30}}));
        ASSERT_TRUE(quick("benchmarkRank", {{"rangeKind", "recentDays"}, {"count", 30}}));
        ASSERT_TRUE(quick("benchmarkPlaytime", {{"rangeKind", "recentDays"}, {"count", 30}}));
        record(measure("day rollover (4 today-anchored ranges)", kSamples, [&](int) { env.daySource->advance(1); }));

        // Graph switches: the Graphs page's selector, and the scenario the history graphs follow.
        auto *vm = app->settingsVm();
        record(measure("beginVisit + endVisit (seed the working copy)", kSamples, [&](int) {
            vm->beginVisit();
            vm->endVisit();
        }));
        vm->beginVisit();
        record(measure("settings graph switch (Graphs page selector)", kSamples * 5, [&](const int i) {
            vm->setSelectedGraph(graphs[static_cast<std::size_t>(i) % graphs.size()]);
        }));
        record(measure("settings draft edit + graphDrafts read", kSamples, [&](const int i) {
            ASSERT_TRUE(vm->setGraphField("benchmarkPlaytime", "dataRange", "", "averagingWindow", QString::number(20 + i)));
            (void) vm->graphDrafts();
        }));
        record(measure("settings Save (4 graphs edited)", kSamples, [&](const int i) {
            ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", QString::number(40 + i)));
            ASSERT_TRUE(vm->setGraphField("benchmarkPlaytime", "dataRange", "", "averagingWindow", QString::number(50 + i)));
            ASSERT_TRUE(vm->setGraphConnection("scenarioHistory", i % 2 == 0 ? "straight" : "pointsOnly"));
            ASSERT_TRUE(vm->setGraphConnection("benchmarkRank", i % 2 == 0 ? "straight" : "pointsOnly"));
            ASSERT_TRUE(vm->save()["accepted"].toBool());
        }));
        vm->endVisit();
        record(measure("scenario switch (history, run graph; Heavy <-> Scenario 001)", kSamples, [&](const int i) {
            if (i % 2 == 0) app->scenarioBrowserVm()->activateScenario("hash-001", "Scenario 001");
            else app->scenarioBrowserVm()->activateScenario(kHeavyHash, "Heavy Scenario");
        }));

        std::printf("\nGRAPH-CONFIGURATION TIMINGS  scenarios=%d runs=%d days=%d  profile load=%.1f ms\n", kScenarios, runs,
                    kProfileDays, loadMs);
        std::printf("%-84s %10s %10s %10s %4s\n", "change", "cold ms", "median ms", "max ms", "n");
        for (const auto &timing: timings) {
            std::printf("%-84s %10.2f %10.2f %10.2f %4d\n", timing.name.c_str(), timing.coldMs, timing.warmMedianMs,
                        timing.warmMaxMs, timing.samples);
            const auto index = std::to_string(&timing - timings.data());
            RecordProperty("timing_us_cold_" + index, static_cast<int>(timing.coldMs * 1000));
            RecordProperty("timing_us_median_" + index, static_cast<int>(timing.warmMedianMs * 1000));
        }
        std::fflush(stdout);
        RecordProperty("profile_runs", runs);
        RecordProperty("profile_scenarios", kScenarios);
        RecordProperty("profile_load_ms", static_cast<int>(loadMs));
        EXPECT_FALSE(timings.empty());
    }

    // As shipped: every accepted change is written to the QSettings document before it returns.
    TEST_F(LargeProfileTest, RecordsHowLongEachConfigurationChangeTakes) { recordTimings("QSettings persistence"); }

    // The same changes with the document held in memory, which isolates the calculation from the write.
    class LargeProfileInMemoryTest : public LargeProfileTest {
    protected:
        tests_support::FakeGraphConfigurationPersistence memory;

        std::shared_ptr<data::IGraphConfigurationPersistence> persistence() override {
            return {&memory, [](data::IGraphConfigurationPersistence *) {}};
        }
    };

    TEST_F(LargeProfileInMemoryTest, RecordsHowLongEachConfigurationChangeTakesWithoutTheWrite) {
        recordTimings("in-memory persistence");
    }

    // ---- QUAL-03/04: selection, drafts and summaries ride out configuration changes ---------

    TEST_F(LargeProfileTest, SelectionDraftAndBenchmarkSummariesSurviveConfigurationChanges) {
        auto *history = app->completionHistoryVm();
        auto *tracking = app->benchmarkTrackingVm();
        auto *vm = app->settingsVm();
        const auto *useCase = app->benchmarkTrackingUseCase().get();
        ASSERT_NE(useCase->projection(), nullptr);
        const auto summaryBefore = benchmarkSummary(*useCase->projection());
        ASSERT_GT(summaryBefore.size(), static_cast<qsizetype>(kBenchmarkScenarios));

        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "allHistory"}}));
        ASSERT_TRUE(QTest::qWaitFor([&] { return history->visibleRunCount() == kHeavyRuns; }, 3000));
        history->selectRun(750);
        ASSERT_TRUE(QTest::qWaitFor([&] { return history->selectedRunIndex() == 750; }, 3000));
        const auto selectedRun = app->sessionController()->getCurrentRun().run_id;

        vm->beginVisit();
        ASSERT_TRUE(vm->setGraphConnection("benchmarkRank", "pointsOnly"));
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "9"));
        vm->setSelectedGraph("benchmarkRank");

        // Committed changes land while the draft is open; the draft and selection are not disturbed.
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 100}}));
        ASSERT_TRUE(quick("benchmarkRank", {{"rangeKind", "recentDays"}, {"count", 7}}));
        ASSERT_TRUE(quick("benchmarkPlaytime", {{"rangeKind", "recentDays"}, {"count", 7}, {"window", 21}}));
        env.daySource->advance(5);
        tracking->clearSelection();
        tracking->selectBenchmark(kBenchmarkId);
        ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "trackable"; }, 30000));

        EXPECT_TRUE(vm->hasPendingChanges());
        EXPECT_EQ(vm->selectedGraph(), "benchmarkRank");
        EXPECT_EQ(vm->graphDraft("benchmarkRank")["connection"].toString(), "pointsOnly");
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9);
        EXPECT_EQ(history->runCount(), kHeavyRuns);
        EXPECT_EQ(history->visibleRunCount(), 100);
        EXPECT_EQ(history->selectedRunIndex(), 750) << "the selection keeps its place in the full history";
        EXPECT_FALSE(history->selectedRunVisible());
        EXPECT_EQ(app->sessionController()->getCurrentRun().run_id, selectedRun);

        // Summaries are definitions of the full history, whatever windows are drawn.
        ASSERT_NE(useCase->projection(), nullptr);
        EXPECT_EQ(benchmarkSummary(*useCase->projection()), summaryBefore);
        vm->endVisit();
    }
}
