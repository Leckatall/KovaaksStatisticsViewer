//
// ScenarioBrowserViewModel tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include <QAbstractListModel>
#include <QSignalSpy>

#include "scenario_browser_vm.h"
#include "run_list_model.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController : public ISessionController {
    public:
        std::vector<ScenarioId> scenario_list;
        std::vector<ScenarioSummary> scenario_summaries;
        std::vector<RunSummary> runs_for_scenario;
        std::vector<RunSummary> recent_runs;
        std::size_t last_recent_runs_count = 0;
        ScenarioId last_runs_for_scenario_query;
        ScenarioPerf current_perf;
        std::vector<ScenarioRunId> set_current_perf_run_id_calls;

        std::vector<ScenarioId> getScenarioList() override { return scenario_list; }

        void generateProfileFromDirectory() const override {
        }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerfToLatest() override {}
        void setCurrentPerf(const std::string &filename) override {}

        void setCurrentPerf(const ScenarioRunId &run_id) override {
            set_current_perf_run_id_calls.push_back(run_id);
        }

        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override { return scenario_summaries; }

        [[nodiscard]] std::vector<RunSummary> getRunsForScenario(const ScenarioId &scenario) const override {
            const_cast<FakeSessionController *>(this)->last_runs_for_scenario_query = scenario;
            return runs_for_scenario;
        }

        [[nodiscard]] std::vector<RunSummary> getRecentRuns(const std::size_t count) const override {
            const_cast<FakeSessionController *>(this)->last_recent_runs_count = count;
            return recent_runs;
        }
    };

    ScenarioSummary make_summary(const std::string &name, const std::string &hash, const int run_count = 1,
                                  const long long last_played_epoch_seconds = 0) {
        ScenarioSummary summary;
        summary.scenario_id = ScenarioId{.name = name, .hash = hash};
        summary.run_count = run_count;
        summary.last_played = std::chrono::sys_seconds{std::chrono::seconds{last_played_epoch_seconds}};
        return summary;
    }

    std::vector<QString> collectScenarioNames(QAbstractListModel *scenario_model) {
        std::vector<QString> result;
        for (int row = 0; row < scenario_model->rowCount(); ++row)
            result.push_back(scenario_model->data(scenario_model->index(row, 0), ScenarioListModel::NameRole).toString());
        return result;
    }

    RunSummary make_run(const std::string &scenario_name, const std::string &hash, const long long start_time,
                         const float score, const float accuracy) {
        RunSummary run;
        run.run_id = ScenarioRunId{.scenario_id = ScenarioId{.name = scenario_name, .hash = hash}, .start_time = start_time};
        run.scenario_name = QString::fromStdString(scenario_name);
        run.start_time_ms = start_time;
        run.score = score;
        run.accuracy = accuracy;
        run.duration_seconds = 60.0F;
        run.shots = 100;
        run.hits = 80;
        return run;
    }

    RunSummary make_run_full(const std::string &scenario_name, const std::string &hash, const long long start_time,
                              const float score, const float accuracy, const float duration_seconds,
                              const int shots, const int hits) {
        RunSummary run = make_run(scenario_name, hash, start_time, score, accuracy);
        run.duration_seconds = duration_seconds;
        run.shots = shots;
        run.hits = hits;
        return run;
    }

    class ScenarioBrowserViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> fake_controller = std::make_shared<FakeSessionController>();

        [[nodiscard]] static QAbstractListModel *asListModel(ScenarioBrowserViewModel &vm) {
            return qobject_cast<QAbstractListModel *>(vm.scenarioModel());
        }

        [[nodiscard]] static QAbstractListModel *asRunListModel(ScenarioBrowserViewModel &vm) {
            return qobject_cast<QAbstractListModel *>(vm.runModel());
        }

        [[nodiscard]] static QAbstractListModel *asRecentRunsModel(ScenarioBrowserViewModel &vm) {
            return qobject_cast<QAbstractListModel *>(vm.recentRunsModel());
        }
    };

    TEST_F(ScenarioBrowserViewModelTest, ScenarioModelInitiallyContainsAllSummaries) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        EXPECT_EQ(asListModel(view_model)->rowCount(), 2);
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSearchTextFiltersCaseInsensitivelyByName) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
            make_summary("Smoothbot Invincible", "hash-3"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setSearchText("MICRO");

        auto *model = asListModel(view_model);
        ASSERT_EQ(model->rowCount(), 1);
        EXPECT_EQ(model->data(model->index(0, 0), ScenarioListModel::NameRole).toString(), QString("Microshot"));
    }

    TEST_F(ScenarioBrowserViewModelTest, EmptySearchTextRestoresAllScenarios) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setSearchText("micro");
        ASSERT_EQ(asListModel(view_model)->rowCount(), 1);

        view_model.setSearchText("");

        EXPECT_EQ(asListModel(view_model)->rowCount(), 2);
    }

    TEST_F(ScenarioBrowserViewModelTest, ActivateScenarioRecordsActiveHashAndEmitsSignal) {
        ScenarioBrowserViewModel view_model(fake_controller);
        const QSignalSpy spy(&view_model, &ScenarioBrowserViewModel::activeScenarioChanged);

        view_model.activateScenario("hash-2", "Microshot");

        EXPECT_EQ(view_model.activeScenarioHash(), QString("hash-2"));
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(ScenarioBrowserViewModelTest, ActivateScenarioDoesNotEmitWhenHashUnchanged) {
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        const QSignalSpy spy(&view_model, &ScenarioBrowserViewModel::activeScenarioChanged);
        view_model.activateScenario("hash-2", "Microshot");

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(ScenarioBrowserViewModelTest, DefaultScenarioSortIsRunCountDescending) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1", 3),
            make_summary("Microshot", "hash-2", 7),
            make_summary("Smoothbot Invincible", "hash-3", 5),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"Microshot", "Smoothbot Invincible", "1wall6targets TE"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetScenarioSortRunCountAscendingOrdersLowestFirst) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1", 3),
            make_summary("Microshot", "hash-2", 7),
            make_summary("Smoothbot Invincible", "hash-3", 5),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::RUN_COUNT, true);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"1wall6targets TE", "Smoothbot Invincible", "Microshot"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetScenarioSortNameDescendingOrdersReverseAlphabetically) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
            make_summary("Smoothbot Invincible", "hash-3"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::NAME, false);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"Smoothbot Invincible", "Microshot", "1wall6targets TE"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetScenarioSortNameAscendingOrdersAlphabetically) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
            make_summary("Smoothbot Invincible", "hash-3"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::NAME, true);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"1wall6targets TE", "Microshot", "Smoothbot Invincible"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetScenarioSortLastPlayedDescendingOrdersMostRecentFirst) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1", 1, 1000),
            make_summary("Microshot", "hash-2", 1, 3000),
            make_summary("Smoothbot Invincible", "hash-3", 1, 2000),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::LAST_PLAYED, false);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"Microshot", "Smoothbot Invincible", "1wall6targets TE"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetScenarioSortLastPlayedAscendingOrdersLeastRecentFirst) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1", 1, 1000),
            make_summary("Microshot", "hash-2", 1, 3000),
            make_summary("Smoothbot Invincible", "hash-3", 1, 2000),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::LAST_PLAYED, true);

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"1wall6targets TE", "Smoothbot Invincible", "Microshot"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, RefreshReappliesConfiguredScenarioSort) {
        fake_controller->scenario_summaries = {
            make_summary("1wall6targets TE", "hash-1"),
            make_summary("Microshot", "hash-2"),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.setScenarioSort(ScenarioBrowserViewModel::ScenarioSortField::NAME, true);
        ASSERT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"1wall6targets TE", "Microshot"}));

        fake_controller->scenario_summaries.push_back(make_summary("Smoothbot Invincible", "hash-3"));
        view_model.refresh();

        EXPECT_EQ(collectScenarioNames(asListModel(view_model)),
                  (std::vector<QString>{"1wall6targets TE", "Microshot", "Smoothbot Invincible"}));
    }

    TEST_F(ScenarioBrowserViewModelTest, CurrentPerfChangedSignalRefreshesScenarioSummaries) {
        ScenarioBrowserViewModel view_model(fake_controller);
        ASSERT_EQ(asListModel(view_model)->rowCount(), 0);

        fake_controller->scenario_summaries = {make_summary("Microshot", "hash-2")};
        emit fake_controller->currentPerfChanged();

        EXPECT_EQ(asListModel(view_model)->rowCount(), 1);
    }

    TEST_F(ScenarioBrowserViewModelTest, ActivateScenarioPopulatesRunModelNewestFirst) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 2000, 8500.0F, 0.9F),
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.activateScenario("hash-2", "Microshot");

        EXPECT_EQ(fake_controller->last_runs_for_scenario_query.hash, "hash-2");
        auto *run_model = asRunListModel(view_model);
        ASSERT_EQ(run_model->rowCount(), 2);
        EXPECT_EQ(run_model->data(run_model->index(0, 0), RunListModel::StartTimeMsRole).toLongLong(), 2000);
        EXPECT_FLOAT_EQ(run_model->data(run_model->index(0, 0), RunListModel::ScoreRole).toFloat(), 8500.0F);
        EXPECT_FLOAT_EQ(run_model->data(run_model->index(0, 0), RunListModel::AccuracyRole).toFloat(), 0.9F);
        EXPECT_EQ(run_model->data(run_model->index(1, 0), RunListModel::StartTimeMsRole).toLongLong(), 1000);
        EXPECT_EQ(run_model->data(run_model->index(0, 0), RunListModel::DurationSecondsRole).toFloat(), 60.0F);
        EXPECT_EQ(run_model->data(run_model->index(0, 0), RunListModel::ShotsRole).toInt(), 100);
        EXPECT_EQ(run_model->data(run_model->index(0, 0), RunListModel::HitsRole).toInt(), 80);
    }

    TEST_F(ScenarioBrowserViewModelTest, ActivatingEmptyScenarioClearsRunModel) {
        fake_controller->runs_for_scenario = {make_run("Microshot", "hash-2", 1000, 8500.0F, 0.9F)};
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");
        ASSERT_EQ(asRunListModel(view_model)->rowCount(), 1);

        view_model.activateScenario("", "");

        EXPECT_EQ(asRunListModel(view_model)->rowCount(), 0);
    }

    TEST_F(ScenarioBrowserViewModelTest, SelectRunInvokesSetCurrentPerfWithRunId) {
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.selectRun("hash-2", 1723200000000.0);

        ASSERT_EQ(fake_controller->set_current_perf_run_id_calls.size(), 1u);
        const auto &run_id = fake_controller->set_current_perf_run_id_calls.front();
        EXPECT_EQ(run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(run_id.start_time, 1723200000000LL);
    }

    std::vector<long long> collectStartTimes(QAbstractListModel *run_model) {
        std::vector<long long> result;
        for (int row = 0; row < run_model->rowCount(); ++row)
            result.push_back(run_model->data(run_model->index(row, 0), RunListModel::StartTimeMsRole).toLongLong());
        return result;
    }

    TEST_F(ScenarioBrowserViewModelTest, DefaultSortIsDateDescending) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 3000, 8500.0F, 0.9F),
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
            make_run("Microshot", "hash-2", 2000, 7500.0F, 0.85F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        view_model.activateScenario("hash-2", "Microshot");

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{3000, 2000, 1000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortDateAscendingOrdersOldestFirst) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 3000, 8500.0F, 0.9F),
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
            make_run("Microshot", "hash-2", 2000, 7500.0F, 0.85F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Date, true);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{1000, 2000, 3000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortScoreDescendingOrdersHighestFirst) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
            make_run("Microshot", "hash-2", 2000, 9000.0F, 0.9F),
            make_run("Microshot", "hash-2", 3000, 8000.0F, 0.85F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Score, false);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{2000, 3000, 1000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortScoreAscendingOrdersLowestFirst) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
            make_run("Microshot", "hash-2", 2000, 9000.0F, 0.9F),
            make_run("Microshot", "hash-2", 3000, 8000.0F, 0.85F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Score, true);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{1000, 3000, 2000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortAccuracyDescendingHandlesShotsZeroGuard) {
        fake_controller->runs_for_scenario = {
            make_run_full("Microshot", "hash-2", 1000, 7000.0F, 0.8F, 60.0F, 100, 80),
            make_run_full("Microshot", "hash-2", 2000, 9000.0F, 0.0F, 60.0F, 0, 0),
            make_run_full("Microshot", "hash-2", 3000, 8000.0F, 0.95F, 60.0F, 50, 47),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Accuracy, false);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{3000, 1000, 2000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortAccuracyAscendingHandlesShotsZeroGuard) {
        fake_controller->runs_for_scenario = {
            make_run_full("Microshot", "hash-2", 1000, 7000.0F, 0.8F, 60.0F, 100, 80),
            make_run_full("Microshot", "hash-2", 2000, 9000.0F, 0.0F, 60.0F, 0, 0),
            make_run_full("Microshot", "hash-2", 3000, 8000.0F, 0.95F, 60.0F, 50, 47),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Accuracy, true);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{2000, 1000, 3000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortDurationDescendingOrdersLongestFirst) {
        fake_controller->runs_for_scenario = {
            make_run_full("Microshot", "hash-2", 1000, 7000.0F, 0.8F, 45.0F, 100, 80),
            make_run_full("Microshot", "hash-2", 2000, 9000.0F, 0.9F, 90.0F, 100, 90),
            make_run_full("Microshot", "hash-2", 3000, 8000.0F, 0.85F, 60.0F, 100, 85),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Duration, false);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{2000, 3000, 1000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortDurationAscendingOrdersShortestFirst) {
        fake_controller->runs_for_scenario = {
            make_run_full("Microshot", "hash-2", 1000, 7000.0F, 0.8F, 45.0F, 100, 80),
            make_run_full("Microshot", "hash-2", 2000, 9000.0F, 0.9F, 90.0F, 100, 90),
            make_run_full("Microshot", "hash-2", 3000, 8000.0F, 0.85F, 60.0F, 100, 85),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Duration, true);

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{1000, 3000, 2000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SetSortScoreDescendingIsStableForTies) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 3000, 8000.0F, 0.9F),
            make_run("Microshot", "hash-2", 2000, 8000.0F, 0.9F),
            make_run("Microshot", "hash-2", 1000, 8000.0F, 0.9F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");

        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Score, false);

        // All scores tie; stable sort preserves the incoming newest-first order.
        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{3000, 2000, 1000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, SortStatePersistsAcrossScenarioActivation) {
        fake_controller->runs_for_scenario = {
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
            make_run("Microshot", "hash-2", 2000, 9000.0F, 0.9F),
            make_run("Microshot", "hash-2", 3000, 8000.0F, 0.85F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-2", "Microshot");
        view_model.setRunSort(ScenarioBrowserViewModel::RunSortField::Score, false);
        ASSERT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{2000, 3000, 1000}));

        view_model.activateScenario("hash-3", "OtherScenario");
        view_model.activateScenario("hash-2", "Microshot");

        EXPECT_EQ(collectStartTimes(asRunListModel(view_model)), (std::vector<long long>{2000, 3000, 1000}));
    }

    TEST_F(ScenarioBrowserViewModelTest, RecentRunsModelPopulatedNewestFirstAcrossScenarios) {
        fake_controller->recent_runs = {
            make_run("Microshot", "hash-2", 3000, 9000.0F, 0.95F),
            make_run("1wall6targets TE", "hash-1", 2000, 8000.0F, 0.9F),
            make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);

        auto *recent_model = asRecentRunsModel(view_model);
        ASSERT_EQ(recent_model->rowCount(), 3);
        EXPECT_EQ(recent_model->data(recent_model->index(0, 0), RunListModel::StartTimeMsRole).toLongLong(), 3000);
        EXPECT_EQ(recent_model->data(recent_model->index(0, 0), RunListModel::ScenarioNameRole).toString(), QString("Microshot"));
        EXPECT_EQ(recent_model->data(recent_model->index(1, 0), RunListModel::StartTimeMsRole).toLongLong(), 2000);
        EXPECT_EQ(recent_model->data(recent_model->index(1, 0), RunListModel::ScenarioNameRole).toString(), QString("1wall6targets TE"));
        EXPECT_EQ(recent_model->data(recent_model->index(2, 0), RunListModel::StartTimeMsRole).toLongLong(), 1000);
    }

    TEST_F(ScenarioBrowserViewModelTest, RecentRunsModelIsRequestedWithConfiguredCap) {
        ScenarioBrowserViewModel view_model(fake_controller);

        EXPECT_EQ(fake_controller->last_recent_runs_count, 10u);
    }

    TEST_F(ScenarioBrowserViewModelTest, CurrentPerfChangedSignalRefreshesRecentRuns) {
        ScenarioBrowserViewModel view_model(fake_controller);
        ASSERT_EQ(asRecentRunsModel(view_model)->rowCount(), 0);

        fake_controller->recent_runs = {make_run("Microshot", "hash-2", 1000, 7000.0F, 0.8F)};
        emit fake_controller->currentPerfChanged();

        EXPECT_EQ(asRecentRunsModel(view_model)->rowCount(), 1);
    }

    TEST_F(ScenarioBrowserViewModelTest, SelectingRecentRunInvokesSetCurrentPerfAcrossScenarios) {
        fake_controller->recent_runs = {
            make_run("Microshot", "hash-2", 3000, 9000.0F, 0.95F),
        };
        ScenarioBrowserViewModel view_model(fake_controller);
        view_model.activateScenario("hash-1", "1wall6targets TE");

        view_model.selectRun("hash-2", 3000.0);

        ASSERT_EQ(fake_controller->set_current_perf_run_id_calls.size(), 1u);
        const auto &run_id = fake_controller->set_current_perf_run_id_calls.front();
        EXPECT_EQ(run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(run_id.start_time, 3000LL);
    }
}
