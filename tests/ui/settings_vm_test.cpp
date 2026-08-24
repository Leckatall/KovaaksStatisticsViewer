//
// SettingsViewModel tests using a hand-written fake ISettingsService.
//

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <type_traits>

#include "settings_vm.h"
#include "series_expression_editor_model.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::vector<std::string> dirs{"C:/Kovaaks"};
        bool dir_set = true;
        std::string profile_path = "C:/Profile/profile.pb";

        [[nodiscard]] std::vector<std::string> getKovaaksDirs() const override { return dirs; }
        [[nodiscard]] bool isKovaaksDirSet() const override { return dir_set; }

        void setKovaaksDirs(const std::vector<std::string> &new_dirs) override {
            dirs = new_dirs;
            dir_set = true;
        }

        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }
        void setProfilePath(const std::string &new_path) override { profile_path = new_path; }

        void onProfilePathChanged(std::function<void()>) override {
        }

        void onKovaaksDirsChanged(std::function<void()>) override {
        }
        [[nodiscard]] bool hasSeriesConfigDocument() const override { return false; }
        [[nodiscard]] std::string getSeriesConfigDocument() const override { return {}; }
        void setSeriesConfigDocument(const std::string &) override {}
        void quarantineSeriesConfigDocument(const std::string &) override {}
        [[nodiscard]] std::vector<std::string> getLegacyDisabledColumnKeys() const override { return {}; }
    };

    class FakeProfileService : public IProfileService {
    public:
        bool profile_loaded = false;
        std::function<void()> stored_callback;

        void generateProfileFromDirectory() override {
        }

        void loadProfile() override {
        }

        void onBuildRequested(std::function<void()>) override {
        }

        void beginProfileBuild() override {
        }

        void applyBuiltProfile(ksv::domain::UserProfile) override {
        }

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return {}; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(
            const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(
            const ScenarioId &, std::size_t) const override { return {}; }

        [[nodiscard]] std::vector<ScenarioPerf> getRunsForScenario(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::vector<RunData>
        getCompletionHistory(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::optional<float> getAverageScore(
            const ScenarioId &, std::size_t) const override { return std::nullopt; }

        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &) const override { return std::nullopt; }

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(std::size_t) const override { return {}; }

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int) const override { return {}; }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void onProfileChanged(std::function<void()> callback) override { stored_callback = std::move(callback); }
    };

    class FakeSeriesManagementUseCase final : public ISeriesManagementUseCase {
    public:
        [[nodiscard]] std::vector<SeriesConfig> getAll() const override { return configs; }
        MutationResult setSeriesEnabled(const SeriesId id, const bool enabled) override {
            lastSetEnabled = std::pair{id, enabled};
            return {};
        }
        MutationResult createComputed(const CreateComputedSeriesRequest &) override { return {}; }
        MutationResult updateSeries(const UpdateSeriesRequest &) override { return {}; }
        MutationResult removeComputed(SeriesId) override { return {}; }
        MutationResult reorder(const SeriesId id, const uint32_t position) override {
            lastReorder = std::pair{id, position};
            return {};
        }
        [[nodiscard]] std::vector<AxisConfig> getAllAxes() const override { return axes; }
        MutationResult createAxis(const CreateAxisRequest &) override { ++createAxisCalls; return {}; }
        MutationResult deleteAxis(AxisId) override { ++deleteAxisCalls; return {}; }
        void onChanged(std::function<void()> callback) override { callback_ = std::move(callback); }

        void beginDraft() override { ++beginDraftCalls; }
        MutationResult commitDraft() override { ++commitDraftCalls; return commitDraftResult; }
        void discardDraft() override { ++discardDraftCalls; }
        [[nodiscard]] bool hasPendingChanges() const override { return pendingChanges; }

        std::vector<SeriesConfig> configs;
        std::vector<AxisConfig> axes;
        std::optional<std::pair<SeriesId, bool>> lastSetEnabled;
        std::optional<std::pair<SeriesId, uint32_t>> lastReorder;
        std::function<void()> callback_;

        int createAxisCalls = 0;
        int deleteAxisCalls = 0;
        int beginDraftCalls = 0;
        int commitDraftCalls = 0;
        int discardDraftCalls = 0;
        bool pendingChanges = false;
        MutationResult commitDraftResult;
    };

    class SettingsViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeSeriesManagementUseCase> seriesManagement = std::make_shared<FakeSeriesManagementUseCase>();
        std::unique_ptr<SettingsViewModel> make_view_model() {
            return std::make_unique<SettingsViewModel>(fake_service, fake_profile_service, seriesManagement);
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
        fake_service->dir_set = false;
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
        fake_service->dir_set = false;
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
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_callback));

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profileLoadedChanged);
        fake_profile_service->profile_loaded = true;
        fake_profile_service->stored_callback();

        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(view_model->isProfileLoaded());
    }

    TEST_F(SettingsViewModelTest, HasNoMainGraphSeriesDependency) {
        auto view_model = std::make_unique<SettingsViewModel>(fake_service, fake_profile_service, seriesManagement);
        EXPECT_TRUE(view_model->getKovaaksDir().isValid());
    }

    TEST_F(SettingsViewModelTest, GetAllSeriesConfigsReturnsEveryRowAsAVariantMap) {
        seriesManagement->configs = {
            SeriesConfig{{1}, {"Score", {{0, 150, 0, 255}, 2.0}, true, 0}, primitive(PrimitiveMetric::Score)},
        };
        const auto view_model = make_view_model();
        const auto result = view_model->getAllSeriesConfigs();
        ASSERT_EQ(result.size(), 1);
        const auto row = result.first().toMap();
        EXPECT_EQ(row["id"].toString(), "1");
        EXPECT_EQ(row["name"].toString(), "Score");
        EXPECT_TRUE(row["enabled"].toBool());
    }

    TEST_F(SettingsViewModelTest, SetSeriesEnabledForwardsToTheSeriesManagementUseCase) {
        const auto view_model = make_view_model();
        view_model->setSeriesEnabled("3", false);
        ASSERT_TRUE(seriesManagement->lastSetEnabled);
        EXPECT_EQ(seriesManagement->lastSetEnabled->first.value, 3U);
        EXPECT_FALSE(seriesManagement->lastSetEnabled->second);
    }

    TEST_F(SettingsViewModelTest, ReorderSeriesForwardsIdAndPosition) {
        const auto view_model = make_view_model();
        view_model->reorderSeries("5", 2);
        ASSERT_TRUE(seriesManagement->lastReorder);
        EXPECT_EQ(seriesManagement->lastReorder->first.value, 5U);
        EXPECT_EQ(seriesManagement->lastReorder->second, 2U);
    }

    TEST_F(SettingsViewModelTest, BeginSeriesDraftForwardsToUseCase) {
        const auto view_model = make_view_model();
        view_model->beginSeriesDraft();
        EXPECT_EQ(seriesManagement->beginDraftCalls, 1);
    }

    TEST_F(SettingsViewModelTest, DiscardSeriesDraftForwardsToUseCase) {
        const auto view_model = make_view_model();
        view_model->discardSeriesDraft();
        EXPECT_EQ(seriesManagement->discardDraftCalls, 1);
    }

    TEST_F(SettingsViewModelTest, CommitSeriesDraftForwardsAndEmitsPendingChangesChanged) {
        const auto view_model = make_view_model();
        const QSignalSpy spy(view_model.get(), &SettingsViewModel::pendingChangesChanged);

        view_model->commitSeriesDraft();

        EXPECT_EQ(seriesManagement->commitDraftCalls, 1);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, PendingChangesReflectsUseCaseState) {
        seriesManagement->pendingChanges = true;
        const auto view_model = make_view_model();
        EXPECT_TRUE(view_model->hasPendingChanges());
    }

    TEST_F(SettingsViewModelTest, PendingChangesChangedFiresAlongsideSeriesConfigurationChanged) {
        const auto view_model = make_view_model();
        ASSERT_TRUE(static_cast<bool>(seriesManagement->callback_));

        const QSignalSpy configSpy(view_model.get(), &SettingsViewModel::seriesConfigurationChanged);
        const QSignalSpy pendingSpy(view_model.get(), &SettingsViewModel::pendingChangesChanged);
        seriesManagement->callback_();

        EXPECT_EQ(configSpy.count(), 1);
        EXPECT_EQ(pendingSpy.count(), 1);
    }

    TEST_F(SettingsViewModelTest, BeginExpressionEditReturnsEditorSeededFromMatchingSeriesExpression) {
        seriesManagement->configs = {
            SeriesConfig{{3}, {"Hits", {{0, 0, 0, 255}, 2.0}, true, 0}, primitive(PrimitiveMetric::Hits)},
        };
        const auto view_model = make_view_model();
        std::unique_ptr<SeriesExpressionEditorModel> editor(view_model->beginExpressionEdit("3"));

        ASSERT_NE(editor, nullptr);
        auto *root = qobject_cast<EditablePrimitiveNode *>(editor->root());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->metric(), "hits");
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
}
