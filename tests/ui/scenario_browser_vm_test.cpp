//
// ScenarioBrowserViewModel tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include <QAbstractListModel>
#include <QSignalSpy>

#include "scenario_browser_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController : public ISessionController {
    public:
        std::vector<ScenarioId> scenario_list;
        std::vector<ScenarioSummary> scenario_summaries;
        ScenarioPerf current_perf;

        std::vector<ScenarioId> getScenarioList() override { return scenario_list; }

        void generateProfileFromDirectory() const override {
        }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerf(const std::string &filename) override {}
        void setCurrentPerf(const ScenarioRunId &) override {}
        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override { return scenario_summaries; }

        [[nodiscard]] std::vector<RunSummary> getRunsForScenario(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::vector<RunSummary> getRecentRuns(std::size_t) const override { return {}; }
    };

    ScenarioSummary make_summary(const std::string &name, const std::string &hash, const int run_count = 1) {
        ScenarioSummary summary;
        summary.scenario_id = ScenarioId{.name = name, .hash = hash};
        summary.run_count = run_count;
        return summary;
    }

    class ScenarioBrowserViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> fake_controller = std::make_shared<FakeSessionController>();

        [[nodiscard]] static QAbstractListModel *asListModel(ScenarioBrowserViewModel &vm) {
            return qobject_cast<QAbstractListModel *>(vm.scenarioModel());
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

    TEST_F(ScenarioBrowserViewModelTest, CurrentPerfChangedSignalRefreshesScenarioSummaries) {
        ScenarioBrowserViewModel view_model(fake_controller);
        ASSERT_EQ(asListModel(view_model)->rowCount(), 0);

        fake_controller->scenario_summaries = {make_summary("Microshot", "hash-2")};
        emit fake_controller->currentPerfChanged();

        EXPECT_EQ(asListModel(view_model)->rowCount(), 1);
    }
}
