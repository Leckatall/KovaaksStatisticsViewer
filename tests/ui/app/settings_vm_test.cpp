//
// SettingsViewModel tests using hand-written fakes of its settings, profile and configuration ports.
//

#include <gtest/gtest.h>

#include <chrono>

#include <QSignalSpy>

#include "app/contracts/expression_dsl.h"
#include "presentation/app/settings_vm.h"
#include "presentation/expression/series_expression_editor_model.h"
#include "fake_file_service.h"
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
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();

        std::unique_ptr<SettingsViewModel> make_view_model() {
            return std::make_unique<SettingsViewModel>(fake_service, fake_file_service, fake_profile_service, configuration);
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

        // These read the committed configuration as it stands when called: take them before an edit
        // or save that replaces it.
        [[nodiscard]] const RunSeriesDocument &committedLineup() const { return configuration->configuration.runSeries; }

        [[nodiscard]] qsizetype lineCount() const { return static_cast<qsizetype>(committedLineup().series.size()); }

        [[nodiscard]] qsizetype axisCount() const { return static_cast<qsizetype>(committedLineup().axes.size()); }

        [[nodiscard]] QString lineId(const std::size_t position) const {
            return QString::number(committedLineup().series.at(position).id.value);
        }

        [[nodiscard]] QString lastLineId() const {
            return QString::number(committedLineup().series.back().id.value);
        }

        [[nodiscard]] QString lastAxisId() const { return QString::number(committedLineup().axes.back().id.value); }

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

    TEST_F(SettingsViewModelTest, KovaaksDirValidReflectsTheFileService) {
        fake_file_service->source_roots_valid = false;
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->isKovaaksDirValid());
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirPreservesAdditionalDirectories) {
        fake_service->dirs = {"C:/Primary", "D:/Secondary"};
        const auto view_model = make_view_model();

        view_model->setKovaaksDir(QUrl::fromLocalFile("E:/Replacement"));

        EXPECT_EQ(fake_service->dirs, (std::vector<std::string>{"E:/Replacement", "D:/Secondary"}));
    }

    TEST_F(SettingsViewModelTest, SettingAValidKovaaksDirNotifiesThatValidityChanged) {
        fake_file_service->source_roots_valid = false;
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);

        fake_file_service->source_roots_valid = true;
        view_model->setKovaaksDir(QUrl::fromLocalFile("D:/NewDir"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(view_model->isKovaaksDirValid());
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
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), lineCount());
        EXPECT_EQ(view_model->getAllAxes().size(), axisCount());
    }

    TEST_F(SettingsViewModelTest, OutsideAVisitEditsAreRefusedAndNothingIsSaved) {
        const auto view_model = make_view_model();

        EXPECT_FALSE(view_model->createComputedSeries("Line", "#123456", 2.0, true, "")["succeeded"].toBool());
        view_model->setRunAxisLabelSeriesId(lineId(1));

        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), lineCount());
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "");
    }

    TEST_F(SettingsViewModelTest, BeginVisitSeedsACleanWorkingCopyFromTheCommittedConfiguration) {
        configuration->configuration.runAxisLabelSeriesId = committedLineup().series.back().id;
        const auto view_model = make_view_model();
        const QSignalSpy visitSpy(view_model.get(), &SettingsViewModel::visitOpenChanged);

        view_model->beginVisit();

        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_EQ(visitSpy.count(), 1);
        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), lastLineId());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), lineCount());
    }

    // Reopening or focusing an already open Settings window calls beginVisit() again; it must not
    // reseed over the visit's edits, even when the committed configuration moved on meanwhile.
    TEST_F(SettingsViewModelTest, BeginVisitOnAnOpenVisitNeverReseeds) {
        const auto view_model = make_view_model();
        const auto seededLineCount = lineCount();
        view_model->beginVisit();
        const auto created = view_model->createComputedSeries("Draft", "#123456", 2.0, true, "");
        configuration->configuration.runSeries.series.pop_back();

        view_model->beginVisit();

        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_TRUE(rowIds(*view_model).contains(created["createdId"].toString()));
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), seededLineCount + 1);
    }

    TEST_F(SettingsViewModelTest, DraftEditsNeverReachTheApplicationBoundary) {
        const auto view_model = make_view_model();
        const auto committed = configuration->configuration;
        const auto axesBefore = axisCount();
        const auto axisLine = lineId(0);
        const auto disabledLine = lineId(1);
        const auto reorderedLine = lineId(2);
        view_model->beginVisit();

        const auto created = view_model->createComputedSeries("Draft", "#123456", 2.0, true, "Add(SCORE, HITS)");
        ASSERT_TRUE(created["succeeded"].toBool());
        const auto createdId = created["createdId"].toString();
        const auto axis = view_model->createAxis("Draft axis");
        ASSERT_TRUE(axis["succeeded"].toBool());
        ASSERT_TRUE(view_model->updateSeriesAxis(axisLine, axis["createdAxisId"].toString())["succeeded"].toBool());
        ASSERT_TRUE(view_model->setSeriesEnabled(disabledLine, false)["succeeded"].toBool());
        ASSERT_TRUE(view_model->reorderSeries(reorderedLine, 0)["succeeded"].toBool());
        ASSERT_TRUE(view_model->removeComputedSeries(createdId)["succeeded"].toBool());
        view_model->setRunAxisLabelSeriesId(reorderedLine);

        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_TRUE(configuration->patches.empty());
        EXPECT_EQ(configuration->configuration, committed);
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), reorderedLine);
        EXPECT_EQ(row(*view_model, axisLine)["yAxisId"].toString(), axis["createdAxisId"].toString());
        EXPECT_FALSE(row(*view_model, disabledLine)["enabled"].toBool());
        EXPECT_EQ(row(*view_model, reorderedLine)["displayPosition"].toInt(), 0);
        EXPECT_FALSE(rowIds(*view_model).contains(createdId));
        EXPECT_EQ(view_model->getAllAxes().size(), axesBefore + 1);
    }

    TEST_F(SettingsViewModelTest, SettingHiddenLeadingPointsEditsTheWorkingCopyAndFlagsPendingChanges) {
        constexpr uint32_t kStarting = 1;
        constexpr int kEdited = 3;
        configuration->configuration.runSeries.series.back().presentation.hiddenLeadingPoints = kStarting;
        const auto id = lastLineId();
        const auto view_model = make_view_model();
        view_model->beginVisit();
        ASSERT_FALSE(view_model->hasPendingChanges());
        EXPECT_EQ(row(*view_model, id)["hiddenLeadingPoints"].toInt(), static_cast<int>(kStarting));
        const auto expression = row(*view_model, id)["expression"].toString();

        ASSERT_TRUE(view_model->setSeriesHiddenLeadingPoints(id, kEdited)["succeeded"].toBool());

        EXPECT_EQ(row(*view_model, id)["hiddenLeadingPoints"].toInt(), kEdited);
        EXPECT_TRUE(view_model->hasPendingChanges());
        ASSERT_TRUE(view_model->updateComputedSeries(id, "Renamed", "#123456", 2.0, true, expression)["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, id)["hiddenLeadingPoints"].toInt(), kEdited);
    }

    TEST_F(SettingsViewModelTest, HiddenLeadingPointsOutsideTheAllowedRangeAreRejected) {
        constexpr uint32_t kStarting = 1;
        configuration->configuration.runSeries.series.back().presentation.hiddenLeadingPoints = kStarting;
        const auto id = lastLineId();
        const auto view_model = make_view_model();
        view_model->beginVisit();

        EXPECT_FALSE(view_model->setSeriesHiddenLeadingPoints(id, -1)["succeeded"].toBool());
        EXPECT_FALSE(view_model->setSeriesHiddenLeadingPoints(
            id, static_cast<int>(kMaximumHiddenLeadingPoints) + 1)["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, id)["hiddenLeadingPoints"].toInt(), static_cast<int>(kStarting));
        EXPECT_EQ(view_model->property("maximumHiddenLeadingPoints").toInt(),
                  static_cast<int>(kMaximumHiddenLeadingPoints));
    }

    TEST_F(SettingsViewModelTest, EditsNotifyTheEditorAndTheDirtyState) {
        const auto view_model = make_view_model();
        view_model->beginVisit();
        const QSignalSpy configSpy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);
        const QSignalSpy pendingSpy(view_model.get(), &SettingsViewModel::pendingChangesChanged);

        view_model->setSeriesEnabled(lineId(1), false);
        view_model->setSeriesEnabled(lineId(2), false);

        EXPECT_EQ(configSpy.count(), 2);
        EXPECT_EQ(pendingSpy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, UndoingAnEditByHandLeavesTheVisitClean) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        view_model->setSeriesEnabled(lineId(1), false);
        view_model->setSeriesEnabled(lineId(1), true);

        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, SaveSubmitsTheWholeWorkingCopyOnceAndKeepsTheVisitOpenAndClean) {
        const auto view_model = make_view_model();
        const auto seededLineCount = committedLineup().series.size();
        view_model->beginVisit();
        const auto created = view_model->createComputedSeries("Saved", "#123456", 2.0, true, "");
        view_model->setRunAxisLabelSeriesId(created["createdId"].toString());

        const auto result = view_model->save();

        EXPECT_TRUE(result["accepted"].toBool());
        ASSERT_EQ(configuration->saves.size(), 1U);
        const auto &candidate = configuration->saves.front();
        EXPECT_EQ(candidate.runSeries.series.size(), seededLineCount + 1);
        ASSERT_TRUE(candidate.runAxisLabelSeriesId);
        EXPECT_EQ(QString::number(candidate.runAxisLabelSeriesId->value), created["createdId"].toString());
        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_FALSE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, SaveCarriesPreferencesCommittedBeforeTheVisit) {
        configuration->configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 9;
        const auto view_model = make_view_model();
        view_model->beginVisit();
        view_model->setSeriesEnabled(lineId(1), false);

        view_model->save();

        ASSERT_EQ(configuration->saves.size(), 1U);
        EXPECT_EQ(configuration->saves.front().preferences(GraphKey::DailyPlaytime).averagingWindowDays, 9);
    }

    TEST_F(SettingsViewModelTest, RevertRestoresTheLastSuccessfulSaveRatherThanTheInitialSeed) {
        const auto view_model = make_view_model();
        const auto labelLine = lineId(1);
        view_model->beginVisit();
        const auto saved = view_model->createComputedSeries("Saved", "#123456", 2.0, true, "");
        ASSERT_TRUE(view_model->save()["accepted"].toBool());
        const auto unsaved = view_model->createComputedSeries("Unsaved", "#123456", 2.0, true, "");
        view_model->setRunAxisLabelSeriesId(labelLine);

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
        const auto disabledLine = lineId(1);
        view_model->beginVisit();
        view_model->setSeriesEnabled(disabledLine, false);
        const QSignalSpy issuesSpy(view_model.get(), &SettingsViewModel::issuesChanged);

        const auto result = view_model->save();

        EXPECT_FALSE(result["accepted"].toBool());
        EXPECT_EQ(issuesSpy.count(), 1);
        EXPECT_TRUE(view_model->isVisitOpen());
        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_FALSE(row(*view_model, disabledLine)["enabled"].toBool());
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
        const auto firstDisabledLine = lineId(1);
        const auto secondDisabledLine = lineId(2);
        view_model->beginVisit();
        view_model->setSeriesEnabled(firstDisabledLine, false);
        view_model->save();
        ASSERT_EQ(view_model->issues().size(), 1);

        configuration->rejectSavesWith.clear();
        ASSERT_TRUE(view_model->save()["accepted"].toBool());
        EXPECT_TRUE(view_model->issues().isEmpty());

        configuration->rejectSavesWith = {windowIssue()};
        view_model->setSeriesEnabled(secondDisabledLine, false);
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
        const auto disabledLine = lineId(1);
        const auto labelLine = lineId(2);
        view_model->beginVisit();
        view_model->setSeriesEnabled(disabledLine, false);
        view_model->setSelectedGraph("scenarioHistory");
        view_model->setRawInput("scenarioHistory", "dataRange", "", "latestRuns", "ten");
        view_model->setSelectedGraph("run");
        view_model->setRunAxisLabelSeriesId(labelLine);

        view_model->beginVisit();
        view_model->setSelectedGraph("scenarioHistory");

        EXPECT_FALSE(row(*view_model, disabledLine)["enabled"].toBool());
        EXPECT_EQ(view_model->rawInput("scenarioHistory", "dataRange", "", "latestRuns").toString(), "ten");
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), labelLine);
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
        const auto lineCountBefore = lineCount();
        const auto disabledLine = lineId(1);

        configuration->configuration.runSeries.series.pop_back();
        configuration->publish({}, true, false);
        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), lineCountBefore - 1);

        view_model->beginVisit();
        view_model->setSeriesEnabled(disabledLine, false);
        const auto during = spy.count();
        configuration->publish({}, false, true);
        EXPECT_EQ(spy.count(), during);
        EXPECT_FALSE(row(*view_model, disabledLine)["enabled"].toBool());
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

        const auto createdId = created["createdId"].toString();
        ASSERT_FALSE(createdId.isEmpty());
        EXPECT_EQ(row(*view_model, createdId)["expression"].toString(), "Add(SCORE, HITS)");
        EXPECT_EQ(row(*view_model, createdId)["name"].toString(), "Derived");
    }

    TEST_F(SettingsViewModelTest, RestoreDefaultGraphLinesStagesTheDefaultDocumentAndClearsTheAxisLabel) {
        const auto view_model = make_view_model();
        const auto removedLine = lineId(2);
        QStringList defaultIds;
        for (const auto &config: defaultRunSeriesDocument().series) defaultIds.append(QString::number(config.id.value));
        view_model->beginVisit();
        ASSERT_TRUE(view_model->removeComputedSeries(removedLine)["succeeded"].toBool());
        view_model->setRunAxisLabelSeriesId(removedLine);
        const QSignalSpy configSpy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);

        const auto result = view_model->restoreDefaultGraphLines();

        EXPECT_TRUE(result["succeeded"].toBool());
        EXPECT_GE(configSpy.count(), 1);
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "");
        EXPECT_EQ(rowIds(*view_model), defaultIds);
        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_TRUE(configuration->saves.empty());
    }

    TEST_F(SettingsViewModelTest, RestoreDefaultGraphLinesOverCustomisedLinesIsPendingAndRevertible) {
        const auto defaultLineCount = static_cast<qsizetype>(defaultRunSeriesDocument().series.size());
        configuration->configuration.runSeries.series.pop_back();
        configuration->configuration.runAxisLabelSeriesId = committedLineup().series.front().id;
        const auto labelLine = lineId(0);
        const auto view_model = make_view_model();
        view_model->beginVisit();
        ASSERT_FALSE(view_model->hasPendingChanges());

        ASSERT_TRUE(view_model->restoreDefaultGraphLines()["succeeded"].toBool());

        EXPECT_TRUE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), defaultLineCount);
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), "");
        EXPECT_TRUE(configuration->saves.empty());

        view_model->revertChanges();

        EXPECT_FALSE(view_model->hasPendingChanges());
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), defaultLineCount - 1);
        EXPECT_EQ(view_model->runAxisLabelSeriesId(), labelLine);
    }

    TEST_F(SettingsViewModelTest, RestoreDefaultGraphLinesIsSavedAsTheDefaultDocument) {
        configuration->configuration.runSeries.series.pop_back();
        const auto view_model = make_view_model();
        view_model->beginVisit();
        ASSERT_TRUE(view_model->restoreDefaultGraphLines()["succeeded"].toBool());

        ASSERT_TRUE(view_model->save()["accepted"].toBool());

        ASSERT_EQ(configuration->saves.size(), 1U);
        EXPECT_EQ(configuration->saves.front().runSeries, defaultRunSeriesDocument());
        EXPECT_FALSE(configuration->saves.front().runAxisLabelSeriesId);
    }

    TEST_F(SettingsViewModelTest, RestoreDefaultGraphLinesIsRefusedOutsideAVisit) {
        configuration->configuration.runSeries.series.pop_back();
        const auto trimmedLineCount = lineCount();
        const auto view_model = make_view_model();
        const QSignalSpy configSpy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);

        EXPECT_FALSE(view_model->restoreDefaultGraphLines()["succeeded"].toBool());

        EXPECT_EQ(configSpy.count(), 0);
        EXPECT_EQ(view_model->getAllSeriesConfigs().size(), trimmedLineCount);
        EXPECT_TRUE(configuration->saves.empty());
    }

    TEST_F(SettingsViewModelTest, UpdateComputedSeriesTreatsEmptyDslAsABlankExpression) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto id = lastLineId();

        const auto result = view_model->updateComputedSeries(id, "Derived", "#123456", 2.0, true, "");

        EXPECT_TRUE(result["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, id)["expression"].toString(), "");
        EXPECT_EQ(row(*view_model, id)["name"].toString(), "Derived");
    }

    TEST_F(SettingsViewModelTest, UpdateComputedSeriesRejectsMalformedNonblankDsl) {
        const auto view_model = make_view_model();
        view_model->beginVisit();

        const auto result = view_model->updateComputedSeries(lastLineId(), "Derived", "#123456", 2.0, true, "Add(SCORE)");

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
        const auto axesBefore = axisCount();
        view_model->beginVisit();

        const auto result = view_model->createAxis("Custom");

        const auto createdAxisId = result["createdAxisId"].toString();
        ASSERT_FALSE(createdAxisId.isEmpty());
        EXPECT_EQ(view_model->getAllAxes().size(), axesBefore + 1);
        bool listed = false;
        for (const auto &axis: view_model->getAllAxes()) listed = listed || axis.toMap()["id"].toString() == createdAxisId;
        EXPECT_TRUE(listed);
    }

    TEST_F(SettingsViewModelTest, UpdateSeriesAxisWithAnIdAssignsAndWithEmptyStringClears) {
        const auto view_model = make_view_model();
        const auto line = lineId(0);
        const auto axis = lastAxisId();
        ASSERT_TRUE(committedLineup().series.front().yAxisId != committedLineup().axes.back().id);
        view_model->beginVisit();

        ASSERT_TRUE(view_model->updateSeriesAxis(line, axis)["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, line)["yAxisId"].toString(), axis);

        ASSERT_TRUE(view_model->updateSeriesAxis(line, "")["succeeded"].toBool());
        EXPECT_EQ(row(*view_model, line)["yAxisId"].toString(), "");
    }

    TEST_F(SettingsViewModelTest, ReorderSeriesMovesTheRowToThePosition) {
        const auto view_model = make_view_model();
        const auto line = lastLineId();
        view_model->beginVisit();

        ASSERT_TRUE(view_model->reorderSeries(line, 2)["succeeded"].toBool());

        EXPECT_EQ(row(*view_model, line)["displayPosition"].toInt(), 2);
        EXPECT_FALSE(view_model->reorderSeries(line, -1)["succeeded"].toBool());
    }

    // The expression editor's accepted result arrives through updateComputedSeries() like any other
    // Graph Lines edit, so it lands in the same working copy.
    TEST_F(SettingsViewModelTest, BeginExpressionEditReadsTheWorkingCopy) {
        const auto view_model = make_view_model();
        const auto line = lineId(0);
        view_model->beginVisit();
        view_model->updateComputedSeries(line, "Hits", "#000000", 2.0, true, "HITS");

        std::unique_ptr<SeriesExpressionEditorModel> editor(view_model->beginExpressionEdit(line));

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

        for (const auto key: kGraphKeys) {
            const auto name = QString::fromUtf8(graphKeyName(key).data(), static_cast<qsizetype>(graphKeyName(key).size()));
            const auto &descriptor = graphDescriptor(key);
            const auto entry = quick[name].toMap();

            EXPECT_EQ(entry["hasRange"].toBool(), descriptor.dateRange) << name.toStdString();
            EXPECT_EQ(entry["hasLatestRuns"].toBool(), descriptor.latestRuns) << name.toStdString();
            EXPECT_EQ(entry["hasWindow"].toBool(), descriptor.averagingWindow) << name.toStdString();
        }
        const auto dailyWindow = defaultGraphPreferences(GraphKey::DailyPlaytime).averagingWindowDays;
        ASSERT_TRUE(dailyWindow);
        EXPECT_EQ(quick["dailyPlaytime"].toMap()["window"].toInt(), *dailyWindow);
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
        const auto &startingRange = configuration->configuration.preferences(GraphKey::DailyPlaytime).range;
        ASSERT_TRUE(startingRange);
        const bool startingAnchor = startingRange->anchorToLatestRun;

        const auto result = view_model->applyQuickPatch("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 14}});

        EXPECT_TRUE(result["accepted"].toBool());
        ASSERT_EQ(configuration->patches.size(), 1u);
        EXPECT_EQ(configuration->patches[0].first, GraphKey::DailyPlaytime);
        EXPECT_EQ(configuration->patches[0].second.range, (RangePreference{RecentDays{14}, startingAnchor}));
        EXPECT_TRUE(configuration->saves.empty());
        EXPECT_FALSE(view_model->isVisitOpen());
        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["rangeCount"].toInt(), 14);
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
        const int patchedWindow = kDefaultAveragingWindowDays + 2;
        view_model->applyQuickPatch("dailyPlaytime", {{"window", patchedWindow}});
        view_model->applyQuickPatch("scenarioHistory", {{"connection", "straight"}});

        view_model->beginVisit();
        EXPECT_FALSE(view_model->hasPendingChanges());
        view_model->save();

        ASSERT_EQ(configuration->saves.size(), 1u);
        EXPECT_EQ(configuration->saves[0].preferences(GraphKey::DailyPlaytime).averagingWindowDays, patchedWindow);
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
        const auto windowBefore = view_model->quickSettings()["dailyPlaytime"].toMap()["window"].toInt();

        const auto zero = view_model->applyQuickPatch("dailyPlaytime", {{"window", "0"}});
        const auto huge = view_model->applyQuickPatch("dailyPlaytime", {{"window", "99999999999999999999"}});
        const auto beyond = view_model->applyQuickPatch(
            "dailyPlaytime", {{"window", QString::number(kMaximumAveragingWindowDays + 1)}});

        EXPECT_EQ(zero["issues"].toList().at(0).toMap()["code"].toString(), "nonPositiveCount");
        EXPECT_EQ(huge["issues"].toList().at(0).toMap()["code"].toString(), "countTooLarge");
        EXPECT_EQ(beyond["issues"].toList().at(0).toMap()["code"].toString(), "countTooLarge");
        EXPECT_EQ(view_model->quickSettings()["dailyPlaytime"].toMap()["window"].toInt(), windowBefore);
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
