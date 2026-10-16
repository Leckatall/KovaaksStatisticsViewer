//
// SettingsViewModel tests using hand-written fakes of its settings, profile and configuration ports.
//

#include <gtest/gtest.h>

#include <chrono>

#include <QSignalSpy>

#include "app/contracts/expression_dsl.h"
#include "settings_vm.h"
#include "series_expression_editor_model.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    class SettingsViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_service = [] {
            auto settings = std::make_shared<FakeSettingsService>();
            settings->dirs = {"C:/Kovaaks"};
            settings->profile_path = "C:/Profile/profile.pb";
            return settings;
        }();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();

        std::unique_ptr<SettingsViewModel> make_view_model() {
            return std::make_unique<SettingsViewModel>(fake_service, fake_profile_service, configuration);
        }

        static QVariantMap row(const SettingsViewModel &viewModel, const QString &id) {
            for (const auto &value: viewModel.getAllSeriesConfigs())
                if (value.toMap()["id"].toString() == id) return value.toMap();
            return {};
        }

        static QStringList rowIds(const SettingsViewModel &viewModel) {
            QStringList ids;
            for (const auto &value: viewModel.getAllSeriesConfigs()) ids.append(value.toMap()["id"].toString());
            return ids;
        }

        static GraphConfigurationIssue windowIssue() {
            return {GraphConfigurationIssueCode::NonPositiveCount, GraphKey::DailyPlaytime, "dataRange", "",
                    "averagingWindow"};
        }
    };

    TEST_F(SettingsViewModelTest, KovaaksDirReflectsServiceValueAtConstruction) {
        fake_service->dirs = {"D:/CustomDir"};
        const auto view_model = make_view_model();

        EXPECT_EQ(view_model->getKovaaksDir(), QUrl::fromLocalFile("D:/CustomDir"));
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirUpdatesServiceAndEmitsOnChange) {
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);
        view_model->setKovaaksDir(QUrl::fromLocalFile("D:/NewDir"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getKovaaksDir(), QUrl::fromLocalFile("D:/NewDir"));
        EXPECT_EQ(fake_service->dirs, (std::vector<std::string>{"D:/NewDir"}));
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirDoesNotEmitWhenUnchanged) {
        fake_service->dirs = {"C:/Kovaaks"};
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);
        view_model->setKovaaksDir(QUrl::fromLocalFile("C:/Kovaaks"));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, KovaaksDirSetReflectsServiceValueAtConstruction) {
        fake_service->dirs.clear();
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->isKovaaksDirSet());
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirPreservesAdditionalDirectories) {
        fake_service->dirs = {"C:/Primary", "D:/Secondary"};
        const auto view_model = make_view_model();

        view_model->setKovaaksDir(QUrl::fromLocalFile("E:/Replacement"));

        EXPECT_EQ(fake_service->dirs, (std::vector<std::string>{"E:/Replacement", "D:/Secondary"}));
    }

    TEST_F(SettingsViewModelTest, KovaaksDirSetBecomesTrueAfterSetKovaaksDir) {
        fake_service->dirs.clear();
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);

        view_model->setKovaaksDir(QUrl::fromLocalFile("D:/NewDir"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(view_model->isKovaaksDirSet());
    }

    TEST_F(SettingsViewModelTest, ProfilePathReflectsServiceValueAtConstruction) {
        fake_service->profile_path = "D:/CustomProfile/profile.pb";
        const auto view_model = make_view_model();

        EXPECT_EQ(view_model->getProfilePath(), QUrl::fromLocalFile("D:/CustomProfile/profile.pb"));
    }

    TEST_F(SettingsViewModelTest, SetProfilePathUpdatesSettingsServiceAndEmitsOnChange) {
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profilePathChanged);
        view_model->setProfilePath(QUrl::fromLocalFile("D:/NewProfile/profile.pb"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getProfilePath(), QUrl::fromLocalFile("D:/NewProfile/profile.pb"));
        EXPECT_EQ(fake_service->profile_path, "D:/NewProfile/profile.pb");
    }

    TEST_F(SettingsViewModelTest, SetProfilePathDoesNotEmitWhenUnchanged) {
        fake_service->profile_path = "C:/Profile/profile.pb";
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profilePathChanged);
        view_model->setProfilePath(QUrl::fromLocalFile("C:/Profile/profile.pb"));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, ProfileLoadedReflectsProfileServiceState) {
        fake_profile_service->profile_loaded = true;
        const auto view_model = make_view_model();

        EXPECT_TRUE(view_model->isProfileLoaded());
    }

    TEST_F(SettingsViewModelTest, ProfileLoadedEmitsWhenProfileServiceNotifiesChange) {
        const auto view_model = make_view_model();
        ASSERT_FALSE(fake_profile_service->stored_callbacks.empty());

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profileLoadedChanged);
        fake_profile_service->profile_loaded = true;
        fake_profile_service->notifyProfileChanged();

        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(view_model->isProfileLoaded());
    }

    // ---- Visit lifecycle ----------------------------------------------------------------------

    TEST_F(SettingsViewModelTest, OutsideAVisitRowsAreTheCommittedConfiguration) {
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->isVisitOpen());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), 9);
        EXPECT_EQ(view_model->getAllAxes().size(), 2);
    }

    TEST_F(SettingsViewModelTest, OutsideAVisitEditsAreRefusedAndNothingIsSaved) {
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->createComputedSeries("Line", "#123456", 2.0, true, "")["succeeded"].toBool());
        view_model->setRunAxisLabelSeriesId("2");

        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), 9);
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "");
    }

    TEST_F(SettingsViewModelTest, BeginVisitSeedsACleanWorkingCopyFromTheCommittedConfiguration) {
        configuration->configuration.runAxisLabelSeriesId = SeriesId{7};
        const auto view_model = make_view_model();
        const QSignalSpy visitSpy(view_model.get(), &SettingsViewModel::visitOpenChanged);

        view_model->beginVisit();

        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_EQ(visitSpy.count(), 1);
        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "7");
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), 9);
    }

    // Reopening or focusing an already open Settings window calls beginVisit() again; it must not
    // reseed over the visit's edits, even when the committed configuration moved on meanwhile.
    TEST_F(SettingsViewModelTest, BeginVisitOnAnOpenVisitNeverReseeds) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const auto created = view_model->createComputedSeries("Draft", "#123456", 2.0, true, "");
        configuration->configuration.runSeries.series.pop_back();

        view_model->beginVisit();

        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_TRUE(rowIds(*view_model).contains(created["createdId"].toString()));
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), 10);
    }

    TEST_F(SettingsViewModelTest, DraftEditsNeverReachTheApplicationBoundary) {
        const auto view_model = make_view_model();
        const auto committed = configuration->configuration;
        view_model->beginVisit();

        const auto created = view_model->createComputedSeries("Draft", "#123456", 2.0, true, "Add(SCORE, HITS)");
        ASSERT_TRUE(created["succeeded"].toBool());
        const auto axis = view_model->createAxis("Draft axis");
        ASSERT_TRUE(axis["succeeded"].toBool());
        ASSERT_TRUE(view_model->updateSeriesAxis("1", axis["createdAxisId"].toString())["succeeded"].toBool());
        ASSERT_TRUE(view_model->setSeriesEnabled("2", false)["succeeded"].toBool());
        ASSERT_TRUE(view_model->reorderSeries("3", 0)["succeeded"].toBool());
        ASSERT_TRUE(view_model->removeComputedSeries("9")["succeeded"].toBool());
        view_model->setRunAxisLabelSeriesId("4");

        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_TRUE(configuration->patches.empty());
        EXPECT_EQ(configuration->configuration, committed);
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "4");
        EXPECT_EQ(row(*view_model, "1")["yAxisId"].toString(), axis["createdAxisId"].toString());
        EXPECT_FALSE(row(*view_model, "2")["enabled"].toBool());
        EXPECT_EQ(row(*view_model, "3")["displayPosition"].toInt(), 0);
        EXPECT_FALSE(rowIds(*view_model).contains("9"));
        EXPECT_EQ(view_model->getAllAxes().size(), 3);
    }

    TEST_F(SettingsViewModelTest, EditsNotifyTheEditorAndTheDirtyState) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const QSignalSpy configSpy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);
        const QSignalSpy pendingSpy(view_model.get(), &SettingsViewModel::pendingChangesChanged);

        view_model->setSeriesEnabled("2", false);
        view_model->setSeriesEnabled("3", false);

        EXPECT_EQ(configSpy.count(), 2);
        EXPECT_EQ(pendingSpy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, UndoingAnEditByHandLeavesTheVisitClean) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        view_model->setSeriesEnabled("2", false);
        view_model->setSeriesEnabled("2", true);

        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, SaveSubmitsTheWholeWorkingCopyOnceAndKeepsTheVisitOpenAndClean) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const auto created = view_model->createComputedSeries("Saved", "#123456", 2.0, true, "");
        view_model->setRunAxisLabelSeriesId(created["createdId"].toString());

        const auto result = view_model->save();

        EXPECT_TRUE(result["accepted"].toBool());
        ASSERT_EQ(configuration->saves.size(), 1U);
        const auto &candidate = configuration->saves.front();
        EXPECT_EQ(candidate.runSeries.series.size(), 10U);
        ASSERT_TRUE(candidate.runAxisLabelSeriesId);
        EXPECT_EQ(QString::number(candidate.runAxisLabelSeriesId->value), created["createdId"].toString());
        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, SaveCarriesPreferencesCommittedBeforeTheVisit) {
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 9;
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setSeriesEnabled("2", false);

        view_model->save();

        ASSERT_EQ(configuration->saves.size(), 1U);
        EXPECT_EQ(configuration->saves.front().preferences(GraphKey::DailyPlaytime).averagingWindowDays, 9);
    }

    TEST_F(SettingsViewModelTest, RevertRestoresTheLastSuccessfulSaveRatherThanTheInitialSeed) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const auto saved = view_model->createComputedSeries("Saved", "#123456", 2.0, true, "");
        ASSERT_TRUE(view_model->save()["accepted"].toBool());
        const auto unsaved = view_model->createComputedSeries("Unsaved", "#123456", 2.0, true, "");
        view_model->setRunAxisLabelSeriesId("5");

        view_model->revertChanges();

        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_TRUE(rowIds(*view_model).contains(saved["createdId"].toString()));
        EXPECT_FALSE(rowIds(*view_model).contains(unsaved["createdId"].toString()));
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "");
        EXPECT_EQ(configuration->saves.size(), 1U);
    }

    TEST_F(SettingsViewModelTest, EndingAVisitDropsItsEditsAndTheNextVisitSeedsFromTheLastSave) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const auto saved = view_model->createComputedSeries("Saved", "#123456", 2.0, true, "");
        view_model->save();
        const auto unsaved = view_model->createComputedSeries("Unsaved", "#123456", 2.0, true, "");

        view_model->endVisit();

        EXPECT_FALSE(view_model->isVisitOpen());
        EXPECT_FALSE(view_model->hasPendingChanges());
        view_model->beginVisit();
        EXPECT_TRUE(rowIds(*view_model).contains(saved["createdId"].toString()));
        EXPECT_FALSE(rowIds(*view_model).contains(unsaved["createdId"].toString()));
        EXPECT_EQ(configuration->saves.size(), 1U);
    }

    TEST_F(SettingsViewModelTest, ARejectedSaveRetainsTheWorkingCopyAndReportsEveryIssueByLocation) {
        configuration->rejectSavesWith = {
            windowIssue(),
            {GraphConfigurationIssueCode::UnknownAxis, GraphKey::Run, "graphLines", "records[0].yAxisId", ""}
        };
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setSeriesEnabled("2", false);
        const QSignalSpy issuesSpy(view_model.get(), &SettingsViewModel::issuesChanged);

        const auto result = view_model->save();

        EXPECT_FALSE(result["accepted"].toBool());
        EXPECT_EQ(issuesSpy.count(), 1);
        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_FALSE(row(*view_model, "2")["enabled"].toBool());
        const auto issues = view_model->issues();
        ASSERT_EQ(issues.size(), 2);
        EXPECT_EQ(result["issues"].toList(), issues);
        const auto first = issues[0].toMap();
        EXPECT_EQ(first["graph"].toString(), "dailyPlaytime");
        EXPECT_EQ(first["group"].toString(), "dataRange");
        EXPECT_EQ(first["target"].toString(), "");
        EXPECT_EQ(first["field"].toString(), "averagingWindow");
        EXPECT_EQ(first["code"].toString(), "nonPositiveCount");
        const auto second = issues[1].toMap();
        EXPECT_EQ(second["graph"].toString(), "run");
        EXPECT_EQ(second["group"].toString(), "graphLines");
        EXPECT_EQ(second["target"].toString(), "records[0].yAxisId");
        EXPECT_EQ(second["code"].toString(), "unknownAxis");
    }

    TEST_F(SettingsViewModelTest, ASuccessfulSaveOrARevertClearsEarlierIssues) {
        configuration->rejectSavesWith = {windowIssue()};
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setSeriesEnabled("2", false);
        view_model->save();
        ASSERT_EQ(view_model->issues().size(), 1);

        configuration->rejectSavesWith.clear();
        ASSERT_TRUE(view_model->save()["accepted"].toBool());
        EXPECT_TRUE(view_model->issues().isEmpty());

        configuration->rejectSavesWith = {windowIssue()};
        view_model->setSeriesEnabled("3", false);
        view_model->save();
        ASSERT_EQ(view_model->issues().size(), 1);
        view_model->revertChanges();
        EXPECT_TRUE(view_model->issues().isEmpty());
    }

    // Unparsed text is presentation state, not configuration: it makes the visit dirty, so closing
    // cannot lose it silently, and it blocks Save without the candidate ever being submitted.
    TEST_F(SettingsViewModelTest, RawInputCountsAsDirtyAndBlocksSaveWithAnIssueAtItsField) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const QSignalSpy rawSpy(view_model.get(), &SettingsViewModel::rawInputsChanged);

        view_model->setRawInput("dailyPlaytime", "dataRange", "", "averagingWindow", "3x");

        EXPECT_EQ(rawSpy.count(), 1);
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->rawInput("dailyPlaytime", "dataRange", "", "averagingWindow").toString(), "3x");
        EXPECT_FALSE(view_model->rawInput("scenarioHistory", "dataRange", "", "averagingWindow").isValid());
        ASSERT_EQ(view_model->rawInputs().size(), 1);
        EXPECT_EQ(view_model->rawInputs()[0].toMap()["text"].toString(), "3x");

        const auto result = view_model->save();

        EXPECT_FALSE(result["accepted"].toBool());
        EXPECT_TRUE(configuration->saves.empty());
        ASSERT_EQ(view_model->issues().size(), 1);
        const auto issue = view_model->issues()[0].toMap();
        EXPECT_EQ(issue["graph"].toString(), "dailyPlaytime");
        EXPECT_EQ(issue["field"].toString(), "averagingWindow");
        EXPECT_EQ(issue["code"].toString(), "unparsedInput");
    }

    TEST_F(SettingsViewModelTest, ClearingTheOnlyRawInputLeavesTheVisitCleanAndDropsItsIssue) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setRawInput("dailyPlaytime", "dataRange", "", "averagingWindow", "3x");
        view_model->save();

        view_model->clearRawInput("dailyPlaytime", "dataRange", "", "averagingWindow");

        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_TRUE(view_model->rawInputs().isEmpty());
        EXPECT_TRUE(view_model->issues().isEmpty());
    }

    TEST_F(SettingsViewModelTest, RawInputForAnUnknownGraphOrOutsideAVisitIsIgnored) {
        const auto view_model = make_view_model();
        view_model->setRawInput("dailyPlaytime", "dataRange", "", "averagingWindow", "3x");
        EXPECT_TRUE(view_model->rawInputs().isEmpty());

        view_model->beginVisit();
        view_model->setRawInput("notAGraph", "dataRange", "", "averagingWindow", "3x");
        EXPECT_TRUE(view_model->rawInputs().isEmpty());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    // Graph Lines edits, another graph's unparsed text and the graph selection all live in the one
    // working copy, so moving between graphs and categories (or refocusing the window) loses none.
    TEST_F(SettingsViewModelTest, EditsInSeveralSectionsSurviveGraphSwitchesAndRefocusing) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setSeriesEnabled("2", false);
        view_model->setSelectedGraph("scenarioHistory");
        view_model->setRawInput("scenarioHistory", "dataRange", "", "latestRuns", "ten");
        view_model->setSelectedGraph("run");
        view_model->setRunAxisLabelSeriesId("3");

        view_model->beginVisit();
        view_model->setSelectedGraph("scenarioHistory");

        EXPECT_FALSE(row(*view_model, "2")["enabled"].toBool());
        EXPECT_EQ(view_model->rawInput("scenarioHistory", "dataRange", "", "latestRuns").toString(), "ten");
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "3");
        EXPECT_TRUE(configuration->saves.empty());
    }

    TEST_F(SettingsViewModelTest, SelectedGraphStartsOnTheRunGraphAndIgnoresUnknownKeys) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::selectedGraphChanged);
        EXPECT_EQ(view_model->selectedGraph(), "run");

        view_model->setSelectedGraph("benchmarkRank");
        view_model->setSelectedGraph("benchmarkRank");
        view_model->setSelectedGraph("unknown");

        EXPECT_EQ(view_model->selectedGraph(), "benchmarkRank");
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, ClearingTheAxisLabelFallsBackToTheFirstVisibleSeries) {
        configuration->configuration.runAxisLabelSeriesId = SeriesId{4};
        const auto view_model = make_view_model();
        view_model->beginVisit();

        view_model->setRunAxisLabelSeriesId("");
        view_model->save();

        ASSERT_EQ(configuration->saves.size(), 1U);
        EXPECT_FALSE(configuration->saves.front().runAxisLabelSeriesId);
    }

    TEST_F(SettingsViewModelTest, CommittedChangesRefreshTheEditorOnlyOutsideAVisit) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);

        configuration->configuration.runSeries.series.pop_back();
        configuration->publish({0, {}, true, false});
        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), 8);

        view_model->beginVisit();
        view_model->setSeriesEnabled("2", false);
        const auto during = spy.count();
        configuration->publish({0, {}, false, true});
        EXPECT_EQ(spy.count(), during);
        EXPECT_FALSE(row(*view_model, "2")["enabled"].toBool());
    }

    // ---- Graph Lines rows ---------------------------------------------------------------------

    TEST_F(SettingsViewModelTest, GetAllSeriesConfigsReturnsEveryRowAsAVariantMap) {
        configuration->configuration.runSeries.series = {
            SeriesConfig{{1}, {"Score", {{0, 150, 0, 255}, 2.0}, true, 0}, primitive(PrimitiveMetric::Score)},
        };
        const auto view_model = make_view_model();
        const auto result = view_model->getAllSeriesConfigs();
        ASSERT_EQ(result.size(), 1);
        const auto row = result.first().toMap();
        EXPECT_EQ(row["id"].toString(), "1");
        EXPECT_EQ(row["name"].toString(), "Score");
        EXPECT_TRUE(row["enabled"].toBool());
        EXPECT_EQ(row["expression"].toString(), "SCORE");
    }

    TEST_F(SettingsViewModelTest, GetAllSeriesConfigsEmitsAnEmptyDslStringForBlankExpression) {
        configuration->configuration.runSeries.series = {
            SeriesConfig{{1}, {"Blank", {{0, 150, 0, 255}, 2.0}, true, 0}, {}},
        };
        const auto view_model = make_view_model();

        const auto result = view_model->getAllSeriesConfigs();

        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.first().toMap()["expression"].toString(), "");
    }

    TEST_F(SettingsViewModelTest, GetAllSeriesConfigsIncludesYAxisId) {
        configuration->configuration.runSeries.series = {
            SeriesConfig{SeriesId{7}, {"Score Total", {}, true, 0}, primitive(PrimitiveMetric::Score), AxisId{2}}
        };
        const auto view_model = make_view_model();

        const auto result = view_model->getAllSeriesConfigs();

        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.first().toMap()["yAxisId"].toString(), "2");
    }

    TEST_F(SettingsViewModelTest, GetAllAxesReturnsIdAndName) {
        configuration->configuration.runSeries.axes = {
            AxisConfig{AxisId{2}, "Score Family", {}, AxisTransformKind::Identity}
        };
        const auto view_model = make_view_model();

        const auto result = view_model->getAllAxes();

        ASSERT_EQ(result.size(), 1);
        const auto row = result.first().toMap();
        EXPECT_EQ(row["id"].toString(), "2");
        EXPECT_EQ(row["name"].toString(), "Score Family");
    }

    // ---- Graph Lines edits --------------------------------------------------------------------

    TEST_F(SettingsViewModelTest, CreateComputedSeriesDecodesDslExpression) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto created = view_model->createComputedSeries("Derived", "#123456", 2.0, true, "Add(SCORE, HITS)");

        EXPECT_EQ(created["createdId"].toString(), "10");
        EXPECT_EQ(row(*view_model, "10")["expression"].toString(), "Add(SCORE, HITS)");
        EXPECT_EQ(row(*view_model, "10")["name"].toString(), "Derived");
    }

    TEST_F(SettingsViewModelTest, UpdateComputedSeriesTreatsEmptyDslAsABlankExpression) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto result = view_model->updateComputedSeries("7", "Derived", "#123456", 2.0, true, "");

        EXPECT_TRUE(result["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, "7")["expression"].toString(), "");
        EXPECT_EQ(row(*view_model, "7")["name"].toString(), "Derived");
    }

    TEST_F(SettingsViewModelTest, UpdateComputedSeriesRejectsMalformedNonblankDsl) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto result = view_model->updateComputedSeries("7", "Derived", "#123456", 2.0, true, "Add(SCORE)");

        EXPECT_FALSE(result.value("succeeded").toBool());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, AFailedEditLeavesTheWorkingCopyUntouched) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto result = view_model->removeComputedSeries("99");

        EXPECT_FALSE(result["succeeded"].toBool());
        EXPECT_EQ(result["failure"].toInt(), static_cast<int>(SeriesMutationFailureCode::UnknownSeriesId));
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, CreateAxisReturnsCreatedAxisIdAsAString) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto result = view_model->createAxis("Custom");

        EXPECT_EQ(result["createdAxisId"].toString(), "3");
        EXPECT_EQ(view_model->getAllAxes().size(), 3);
    }

    TEST_F(SettingsViewModelTest, UpdateSeriesAxisWithAnIdAssignsAndWithEmptyStringClears) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        ASSERT_TRUE(view_model->updateSeriesAxis("1", "2")["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, "1")["yAxisId"].toString(), "2");

        ASSERT_TRUE(view_model->updateSeriesAxis("1", "")["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, "1")["yAxisId"].toString(), "");
    }

    TEST_F(SettingsViewModelTest, ReorderSeriesMovesTheRowToThePosition) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        ASSERT_TRUE(view_model->reorderSeries("5", 2)["succeeded"].toBool());

        EXPECT_EQ(row(*view_model, "5")["displayPosition"].toInt(), 2);
        EXPECT_FALSE(view_model->reorderSeries("5", -1)["succeeded"].toBool());
    }

    // The expression editor's accepted result arrives through updateComputedSeries() like any other
    // Graph Lines edit, so it lands in the same working copy.
    TEST_F(SettingsViewModelTest, BeginExpressionEditReadsTheWorkingCopy) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->updateComputedSeries("1", "Hits", "#000000", 2.0, true, "HITS");

        std::unique_ptr<SeriesExpressionEditorModel> editor(view_model->beginExpressionEdit("1"));

        ASSERT_NE(editor, nullptr);
        auto *root = qobject_cast<EditablePrimitiveNode *>(editor->root());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->metric(), "hits");
        EXPECT_TRUE(configuration->saves.empty());
    }

    TEST_F(SettingsViewModelTest, BeginExpressionEditReturnsEditorWithEmptyRootForUnknownId) {
        const auto view_model = make_view_model();
        std::unique_ptr<SeriesExpressionEditorModel> editor(view_model->beginExpressionEdit("999"));

        ASSERT_NE(editor, nullptr);
        EXPECT_EQ(editor->root(), nullptr);
    }

    TEST_F(SettingsViewModelTest, BeginExpressionEditReturnsANewInstanceOnEveryCall) {
        const auto view_model = make_view_model();
        std::unique_ptr<SeriesExpressionEditorModel> first(view_model->beginExpressionEdit("1"));
        std::unique_ptr<SeriesExpressionEditorModel> second(view_model->beginExpressionEdit("1"));

        EXPECT_NE(first.get(), second.get());
    }

    // ---- Quick settings: the committed configuration, bypassing the visit --------------------------

    TEST_F(SettingsViewModelTest, QuickSettingsDescribeWhichControlsEachGraphOffers) {
        const auto view_model = make_view_model();
        const auto quick = view_model->quickSettings();

        const auto run = quick["run"].toMap();
        EXPECT_FALSE(run["hasRange"].toBool());
        EXPECT_FALSE(run["hasLatestRuns"].toBool());
        EXPECT_FALSE(run["hasWindow"].toBool());

        const auto daily = quick["dailyPlaytime"].toMap();
        EXPECT_TRUE(daily["hasRange"].toBool());
        EXPECT_FALSE(daily["hasLatestRuns"].toBool());
        EXPECT_TRUE(daily["hasWindow"].toBool());
        EXPECT_EQ(daily["window"].toInt(), 3);

        const auto history = quick["scenarioHistory"].toMap();
        EXPECT_TRUE(history["hasRange"].toBool());
        EXPECT_TRUE(history["hasLatestRuns"].toBool());
        EXPECT_FALSE(history["hasWindow"].toBool());

        const auto rank = quick["benchmarkRank"].toMap();
        EXPECT_TRUE(rank["hasRange"].toBool());
        EXPECT_FALSE(rank["hasWindow"].toBool());

        EXPECT_TRUE(quick["benchmarkPlaytime"].toMap()["hasWindow"].toBool());
    }

    TEST_F(SettingsViewModelTest, QuickSettingsReportTheDefaultsTruthfully) {
        const auto view_model = make_view_model();
        const auto quick = view_model->quickSettings();

        const auto history = quick["scenarioHistory"].toMap();
        EXPECT_EQ(history["rangeKind"].toString(), "allHistory");
        EXPECT_EQ(history["connection"].toString(), "smooth");
        EXPECT_TRUE(history["markers"].toBool());
        EXPECT_FALSE(quick["dailyPlaytime"].toMap()["markers"].toBool());
    }

    TEST_F(SettingsViewModelTest, QuickSettingsShowACustomRangeAsCustomRatherThanAPreset) {
        using namespace std::chrono;
        configuration->configuration.preferences(GraphKey::DailyPlaytime).range = RangePreference{
            CustomDates{2026y / August / 1d, 2026y / August / 20d}, true
        };
        configuration->configuration.preferences(GraphKey::ScenarioHistory).range = RangePreference{LatestRuns{37}};
        configuration->configuration.preferences(GraphKey::BenchmarkRank).range = RangePreference{RecentDays{45}};
        const auto view_model = make_view_model();
        const auto quick = view_model->quickSettings();

        const auto daily = quick["dailyPlaytime"].toMap();
        EXPECT_EQ(daily["rangeKind"].toString(), "customDates");
        EXPECT_EQ(daily["rangeStart"].toString(), "2026-08-01");
        EXPECT_EQ(daily["rangeEnd"].toString(), "2026-08-20");
        const auto history = quick["scenarioHistory"].toMap();
        EXPECT_EQ(history["rangeKind"].toString(), "latestRuns");
        EXPECT_EQ(history["rangeCount"].toInt(), 37);
        const auto rank = quick["benchmarkRank"].toMap();
        EXPECT_EQ(rank["rangeKind"].toString(), "recentDays");
        EXPECT_EQ(rank["rangeCount"].toInt(), 45);
    }

    // The popup and the Graphs page show and read the same values, so they follow the same locale.
    TEST_F(SettingsViewModelTest, QuickSettingsFormatACustomRangeInTheAppLocaleAndKeepTheIsoDates) {
        using namespace std::chrono;
        configuration->configuration.preferences(GraphKey::DailyPlaytime).range = RangePreference{
            CustomDates{2026y / August / 1d, 2026y / August / 20d}, true
        };
        const auto view_model = make_view_model();

        view_model->setLocale(QLocale(QLocale::German, QLocale::Germany));
        auto daily = view_model->quickSettings()["dailyPlaytime"].toMap();
        EXPECT_EQ(daily["rangeStartFormatted"].toString(), "01.08.2026");
        EXPECT_EQ(daily["rangeEndFormatted"].toString(), "20.08.2026");
        EXPECT_EQ(daily["rangeStart"].toString(), "2026-08-01");

        view_model->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        daily = view_model->quickSettings()["dailyPlaytime"].toMap();
        EXPECT_EQ(daily["rangeStartFormatted"].toString(), "8/1/2026");
        EXPECT_EQ(daily["rangeEndFormatted"].toString(), "8/20/2026");
    }

    TEST_F(SettingsViewModelTest, ChangingTheLocaleRefreshesQuickSettings) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::quickSettingsChanged);

        view_model->setLocale(QLocale(QLocale::German, QLocale::Germany));

        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, QuickSettingsShowCountsInTheAppLocalesDigits) {
        const QLocale arabic(QLocale::Arabic, QLocale::Egypt);
        ASSERT_NE(arabic.toString(1234), "1234") << "the locale must use non-ASCII digits for this test to mean anything";
        configuration->configuration.preferences(GraphKey::ScenarioHistory).range = RangePreference{LatestRuns{1234}};
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 14;
        const auto view_model = make_view_model();
        view_model->setLocale(arabic);

        const auto quick = view_model->quickSettings();

        EXPECT_EQ(quick["scenarioHistory"].toMap()["rangeCountText"].toString(), arabic.toString(1234).remove(arabic.groupSeparator()));
        EXPECT_EQ(quick["dailyPlaytime"].toMap()["windowText"].toString(), arabic.toString(14));
        EXPECT_EQ(quick["scenarioHistory"].toMap()["rangeCount"].toInt(), 1234);
    }

    TEST_F(SettingsViewModelTest, PopupCountsAreReadInTheAppLocalesDigitsLikeThePageReadsThem) {
        const QLocale arabic(QLocale::Arabic, QLocale::Egypt);
        ASSERT_NE(arabic.toString(14), "14");
        const auto view_model = make_view_model();
        view_model->setLocale(arabic);

        const auto window = view_model->applyQuickPatch("dailyPlaytime", {{"window", arabic.toString(14)}});
        const auto latest = view_model->applyQuickPatch(
            "scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", arabic.toString(50)}});

        EXPECT_TRUE(window["accepted"].toBool());
        EXPECT_TRUE(latest["accepted"].toBool());
        ASSERT_EQ(configuration->patches.size(), 2u);
        EXPECT_EQ(configuration->patches[0].second.averagingWindowDays, 14);
        ASSERT_TRUE(configuration->patches[1].second.range.has_value());
        EXPECT_EQ(std::get<LatestRuns>(configuration->patches[1].second.range->range).count, 50);
    }

    TEST_F(SettingsViewModelTest, PopupCountsKeepTheirDistinctRejectionCodesInANonAsciiLocale) {
        const QLocale arabic(QLocale::Arabic, QLocale::Egypt);
        const auto view_model = make_view_model();
        view_model->setLocale(arabic);
        const auto code = [&](const QString &text) {
            const auto result = view_model->applyQuickPatch("dailyPlaytime", {{"window", text}});
            return result["issues"].toList().at(0).toMap()["code"].toString();
        };

        EXPECT_EQ(code(arabic.toString(0)), "nonPositiveCount");
        EXPECT_EQ(code(QString("99999999999999999999")), "countTooLarge");
        EXPECT_EQ(code(arabic.toString(99999).remove(arabic.groupSeparator())), "countTooLarge");
        EXPECT_EQ(code(QString("-") + arabic.toString(5)), "nonPositiveCount");
        EXPECT_EQ(code(QString("x")), "unparsedInput");
        EXPECT_EQ(code(QString("1e3")), "unparsedInput");
    }

    TEST_F(SettingsViewModelTest, QuickSettingsFollowACommittedChangeEvenDuringAVisit) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::quickSettingsChanged);

        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 9;
        configuration->publishDiff();

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["window"].toInt(), 9);
    }

    TEST_F(SettingsViewModelTest, QuickSettingsDoNotChangeForAChangeToGraphLinesOnly) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::quickSettingsChanged);

        configuration->configuration.runAxisLabelSeriesId = SeriesId{2};
        configuration->publishDiff();

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, AQuickRangePatchGoesStraightToTheCommittedConfiguration) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::quickSettingsChanged);

        const auto result = view_model->applyQuickPatch("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 30}});

        EXPECT_TRUE(result["accepted"].toBool());
        ASSERT_EQ(configuration->patches.size(), 1u);
        EXPECT_EQ(configuration->patches[0].first, GraphKey::DailyPlaytime);
        EXPECT_EQ(configuration->patches[0].second.range, (RangePreference{RecentDays{30}}));
        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_FALSE(view_model->isVisitOpen());
        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["rangeCount"].toInt(), 30);
    }

    TEST_F(SettingsViewModelTest, AQuickRangePatchKeepsTheStoredLatestRunAnchor) {
        configuration->configuration.preferences(GraphKey::DailyPlaytime).range = RangePreference{AllHistory{}, true};
        const auto view_model = make_view_model();

        view_model->applyQuickPatch("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 7}});

        ASSERT_EQ(configuration->patches.size(), 1u);
        EXPECT_EQ(configuration->patches[0].second.range, (RangePreference{RecentDays{7}, true}));
    }

    TEST_F(SettingsViewModelTest, QuickPatchesCoverAllHistoryLatestRunsWindowConnectionAndMarkers) {
        const auto view_model = make_view_model();

        view_model->applyQuickPatch("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 50}});
        view_model->applyQuickPatch("scenarioHistory", {{"rangeKind", "allHistory"}});
        view_model->applyQuickPatch("dailyPlaytime", {{"window", 7}});
        view_model->applyQuickPatch("dailyPlaytime", {{"connection", "pointsOnly"}, {"markers", true}});

        ASSERT_EQ(configuration->patches.size(), 4u);
        EXPECT_EQ(configuration->patches[0].second.range, (RangePreference{LatestRuns{50}}));
        EXPECT_EQ(configuration->patches[1].second.range, (RangePreference{AllHistory{}}));
        EXPECT_EQ(configuration->patches[2].second.averagingWindowDays, 7);
        EXPECT_EQ(configuration->patches[3].second.connection, ConnectionMode::PointsOnly);
        EXPECT_EQ(configuration->patches[3].second.lineModeMarkers, true);
        const auto daily = view_model->quickSettings()["dailyPlaytime"].toMap();
        EXPECT_EQ(daily["connection"].toString(), "pointsOnly");
        EXPECT_TRUE(daily["markers"].toBool());
    }

    TEST_F(SettingsViewModelTest, QuickChangesMadeBeforeAVisitSeedItsWorkingCopy) {
        const auto view_model = make_view_model();
        view_model->applyQuickPatch("dailyPlaytime", {{"window", 7}});
        view_model->applyQuickPatch("scenarioHistory", {{"connection", "straight"}});

        view_model->beginVisit();
        EXPECT_FALSE(view_model->hasPendingChanges());
        view_model->save();

        ASSERT_EQ(configuration->saves.size(), 1u);
        EXPECT_EQ(configuration->saves[0].preferences(GraphKey::DailyPlaytime).averagingWindowDays, 7);
        EXPECT_EQ(configuration->saves[0].preferences(GraphKey::ScenarioHistory).appearance.connection,
                  ConnectionMode::Straight);
    }

    TEST_F(SettingsViewModelTest, TextualCountsAreParsedAsWholeNumbers) {
        const auto view_model = make_view_model();

        const auto result = view_model->applyQuickPatch("dailyPlaytime", {{"window", " 14 "}});

        EXPECT_TRUE(result["accepted"].toBool());
        ASSERT_EQ(configuration->patches.size(), 1u);
        EXPECT_EQ(configuration->patches[0].second.averagingWindowDays, 14);
    }

    TEST_F(SettingsViewModelTest, UnparsableCountsNeverReachTheUseCase) {
        const auto view_model = make_view_model();

        for (const QString text: {"abc", "2.5", "", "1e3", "3 days"}) {
            const auto result = view_model->applyQuickPatch("dailyPlaytime", {{"window", text}});
            ASSERT_FALSE(result["accepted"].toBool()) << text.toStdString();
            const auto issues = result["issues"].toList();
            ASSERT_EQ(issues.size(), 1) << text.toStdString();
            EXPECT_EQ(issues[0].toMap()["graph"].toString(), "dailyPlaytime");
            EXPECT_EQ(issues[0].toMap()["group"].toString(), "dataRange");
            EXPECT_EQ(issues[0].toMap()["field"].toString(), "averagingWindow");
            EXPECT_EQ(issues[0].toMap()["code"].toString(), "unparsedInput");
        }
        const auto latest = view_model->applyQuickPatch("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", "x"}});
        EXPECT_EQ(latest["issues"].toList().at(0).toMap()["field"].toString(), "latestRuns");

        EXPECT_TRUE(configuration->patches.empty());
    }

    TEST_F(SettingsViewModelTest, OutOfRangeCountsAreRejectedByValidationWithoutChangingTheEffectiveValue) {
        const auto view_model = make_view_model();

        const auto zero = view_model->applyQuickPatch("dailyPlaytime", {{"window", "0"}});
        const auto huge = view_model->applyQuickPatch("dailyPlaytime", {{"window", "99999999999999999999"}});
        const auto beyond = view_model->applyQuickPatch("dailyPlaytime", {{"window", "3651"}});

        EXPECT_EQ(zero["issues"].toList().at(0).toMap()["code"].toString(), "nonPositiveCount");
        EXPECT_EQ(huge["issues"].toList().at(0).toMap()["code"].toString(), "countTooLarge");
        EXPECT_EQ(beyond["issues"].toList().at(0).toMap()["code"].toString(), "countTooLarge");
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["window"].toInt(), 3);
    }

    TEST_F(SettingsViewModelTest, ARejectedPatchReportsTheUseCaseIssuesAndChangesNothing) {
        configuration->rejectPatchesWith = {{GraphConfigurationIssueCode::NotApplicable, GraphKey::Run, "dataRange", "", "range"}};
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::quickSettingsChanged);

        const auto result = view_model->applyQuickPatch("run", {{"rangeKind", "recentDays"}, {"count", 7}});

        EXPECT_FALSE(result["accepted"].toBool());
        EXPECT_EQ(result["issues"].toList().at(0).toMap()["code"].toString(), "notApplicable");
        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, AnUnknownGraphOrConnectionIsRefused) {
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->applyQuickPatch("nonsense", {{"window", 7}})["accepted"].toBool());
        EXPECT_FALSE(view_model->applyQuickPatch("run", {{"connection", "wavy"}})["accepted"].toBool());
        EXPECT_FALSE(view_model->applyQuickPatch("run", {{"rangeKind", "fortnight"}, {"count", 1}})["accepted"].toBool());

        EXPECT_TRUE(configuration->patches.empty());
    }
}
