//
// SessionController tests using hand-written fakes for ISettingsService and
// IProfileService. Needs a QCoreApplication (see qt_test_main.cpp) since
// SessionController/ISessionController are QObjects with a signal.
//

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <unordered_map>

#include "session_controller.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::string dir = "C:/Kovaaks";
        std::string profile_path = "C:/Profile/profile_cache.pb";

        [[nodiscard]] std::string getKovaaksDir() const override { return dir; }
        void setKovaaksDir(const std::string &new_dir) override { dir = new_dir; }
        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }
        void setProfilePath(const std::string &new_path) override { profile_path = new_path; }
        void onProfilePathChanged(std::function<void()>) override {}
        void onKovaaksDirChanged(std::function<void()>) override {}
    };

    class FakeProfileService : public IProfileService {
    public:
        mutable int generate_call_count = 0;
        bool profile_loaded = false;
        std::vector<ScenarioId> scenario_list;
        std::unordered_map<std::string, ScenarioPerf> perf_by_path;
        std::unordered_map<ScenarioRunId, ScenarioPerf> run_by_id;
        std::unordered_map<ScenarioId, std::vector<ScenarioPerf>> most_recent_perfs_by_scenario;
        std::unordered_map<ScenarioId, std::size_t> run_count_by_scenario;
        std::unordered_map<ScenarioId, double> total_time_by_scenario;
        std::vector<ScenarioPerf> recent_runs;
        ScenarioPerf latest_perf;
        std::function<void()> stored_callback;

        void generateProfileFromDirectory() override { ++generate_call_count; }

        void loadProfile() override { ++generate_call_count; }

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return scenario_list; }

        [[nodiscard]] ScenarioPerf getPerf(const std::string &path) const override { return perf_by_path.at(path); }

        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return latest_perf; }

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &scenario,
                                                                     std::size_t) const override {
            const auto it = most_recent_perfs_by_scenario.find(scenario);
            return it == most_recent_perfs_by_scenario.end() ? std::vector<ScenarioPerf>{} : it->second;
        }

        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &run_id) const override {
            const auto it = run_by_id.find(run_id);
            if (it == run_by_id.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &scenario) const override {
            const auto it = run_count_by_scenario.find(scenario);
            if (it == run_count_by_scenario.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &scenario) const override {
            const auto it = total_time_by_scenario.find(scenario);
            if (it == total_time_by_scenario.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(std::size_t count) const override {
            std::vector<ScenarioPerf> result = recent_runs;
            if (result.size() > count) result.resize(count);
            return result;
        }

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>>
        getRollingTimeAverage(int) const override { return {}; }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void onProfileChanged(std::function<void()> callback) override { stored_callback = std::move(callback); }
    };

    ScenarioPerf make_perf(const std::string &hash, const long long start_time, const float score = 0.0F,
                            const int shots = 0, const int hits = 0, const float duration = 0.0F) {
        ScenarioPerf perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = duration;
        perf.add_data(0.0F, SCORE, score);
        if (shots != 0) perf.add_data(0.0F, SHOTS, shots);
        if (hits != 0) perf.add_data(0.0F, HITS, hits);
        return perf;
    }

    class SessionControllerTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_settings_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();

        std::unique_ptr<SessionController> make_controller() {
            return std::make_unique<SessionController>(fake_settings_service, fake_profile_service);
        }
    };

    TEST_F(SessionControllerTest, GetScenarioListDelegatesToProfileService) {
        fake_profile_service->scenario_list = {ScenarioId{.name = "A", .hash = "h1"}};
        const auto controller = make_controller();

        EXPECT_EQ(controller->getScenarioList().size(), 1);
    }

    TEST_F(SessionControllerTest, GenerateProfileFromDirectoryDelegatesToProfileService) {
        const auto controller = make_controller();

        controller->generateProfileFromDirectory();

        EXPECT_EQ(fake_profile_service->generate_call_count, 1);
    }

    TEST_F(SessionControllerTest, ConstructorLoadsLatestPerfFromProfileService) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfEmitsSignalWhenRunIdDiffers) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(make_perf("hash-2", 200));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfDoesNotEmitSignalWhenRunIdUnchanged) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(make_perf("hash-1", 100));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByFilenameDelegatesToProfileServiceGetPerf) {
        fake_profile_service->perf_by_path["some/file.perf"] = make_perf("hash-2", 200);
        const auto controller = make_controller();

        controller->setCurrentPerf(std::string("some/file.perf"));

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, RegistersOnProfileChangedCallbackThatRefreshesLatestPerf) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_callback));

        fake_profile_service->latest_perf = make_perf("hash-2", 200);
        fake_profile_service->stored_callback();

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdResolvesViaProfileServiceGetRun) {
        const auto run_id = ScenarioRunId{.scenario_id = {.name = "Scenario hash-2", .hash = "hash-2"}, .start_time = 200};
        fake_profile_service->run_by_id[run_id] = make_perf("hash-2", 200);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(run_id);

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdDoesNothingWhenRunNotFound) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        const auto unknown_run_id = ScenarioRunId{.scenario_id = {.name = "?", .hash = "unknown"}, .start_time = 999};
        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(unknown_run_id);

        EXPECT_EQ(spy.count(), 0);
        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, GetScenarioSummariesCombinesScenarioListRunCountAndTotalTime) {
        const auto scenario = ScenarioId{.name = "1wall6", .hash = "hash-1"};
        fake_profile_service->scenario_list = {scenario};
        fake_profile_service->run_count_by_scenario[scenario] = 3;
        fake_profile_service->total_time_by_scenario[scenario] = 42.5;
        const auto controller = make_controller();

        const auto summaries = controller->getScenarioSummaries();

        ASSERT_EQ(summaries.size(), 1);
        EXPECT_EQ(summaries[0].scenario_id.hash, "hash-1");
        EXPECT_EQ(summaries[0].run_count, 3);
        EXPECT_DOUBLE_EQ(summaries[0].total_time_seconds, 42.5);
    }

    TEST_F(SessionControllerTest, GetRunsForScenarioMapsScoreAccuracyAndDuration) {
        const auto scenario = ScenarioId{.name = "1wall6", .hash = "hash-1"};
        fake_profile_service->run_count_by_scenario[scenario] = 1;
        fake_profile_service->most_recent_perfs_by_scenario[scenario] = {
            make_perf("hash-1", 100, 50.0F, 10, 5, 30.0F)
        };
        const auto controller = make_controller();

        const auto runs = controller->getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 1);
        EXPECT_FLOAT_EQ(runs[0].score, 50.0F);
        EXPECT_FLOAT_EQ(runs[0].accuracy, 0.5F);
        EXPECT_FLOAT_EQ(runs[0].duration_seconds, 30.0F);
        EXPECT_EQ(runs[0].shots, 10);
        EXPECT_EQ(runs[0].hits, 5);
        EXPECT_EQ(runs[0].start_time_ms, 100);
    }

    TEST_F(SessionControllerTest, GetRunsForScenarioAccuracyIsZeroWhenNoShots) {
        const auto scenario = ScenarioId{.name = "1wall6", .hash = "hash-1"};
        fake_profile_service->run_count_by_scenario[scenario] = 1;
        fake_profile_service->most_recent_perfs_by_scenario[scenario] = {
            make_perf("hash-1", 100, 50.0F, 0, 0)
        };
        const auto controller = make_controller();

        const auto runs = controller->getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 1);
        EXPECT_FLOAT_EQ(runs[0].accuracy, 0.0F);
    }

    TEST_F(SessionControllerTest, GetRunsForScenarioReturnsNewestFirst) {
        // getMostRecentPerfs (mirroring UserProfile) returns oldest-of-the-window first;
        // getRunsForScenario must present newest-first for the UI.
        const auto scenario = ScenarioId{.name = "1wall6", .hash = "hash-1"};
        fake_profile_service->run_count_by_scenario[scenario] = 2;
        fake_profile_service->most_recent_perfs_by_scenario[scenario] = {
            make_perf("hash-1", 100), make_perf("hash-1", 200)
        };
        const auto controller = make_controller();

        const auto runs = controller->getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 2);
        EXPECT_EQ(runs[0].start_time_ms, 200);
        EXPECT_EQ(runs[1].start_time_ms, 100);
    }

    TEST_F(SessionControllerTest, GetRecentRunsMapsDtoFieldsIncludingScenarioName) {
        fake_profile_service->recent_runs = {make_perf("hash-1", 300, 75.0F, 20, 15, 45.0F)};
        const auto controller = make_controller();

        const auto runs = controller->getRecentRuns(5);

        ASSERT_EQ(runs.size(), 1);
        EXPECT_EQ(runs[0].scenario_name, QString("Scenario hash-1"));
        EXPECT_FLOAT_EQ(runs[0].score, 75.0F);
        EXPECT_FLOAT_EQ(runs[0].accuracy, 0.75F);
    }
}
