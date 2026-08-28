//
// Exercises the REAL composition root (App::App) end to end: the wired graph
// ProfileService -> SessionController and the onProfileChanged -> PlaytimeVm
// refresh cascade that only ever gets assembled at runtime. Uses App's own
// accessors so the test can never drift from the actual wiring.
//

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <memory>
#include <type_traits>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/playtime_graph_vm.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    class CompositionRootTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("VT FlyTS Novice S5.perf").isEmpty());

            // App::App() runs loadProfile(), which asks SessionController for a build
            // on its worker thread when no store is available. The result only lands once the event
            // loop spins, so every test here starts by waiting for it.
            app = std::make_unique<application::App>(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
            ASSERT_TRUE(waitForProfile());
        }

        [[nodiscard]] bool waitForProfile() const {
            QElapsedTimer timer;
            timer.start();
            while (!app->profileService()->isProfileLoaded()) {
                if (timer.elapsed() > 5000) return false;
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
            return true;
        }
    };

    TEST_F(CompositionRootTest, ConstructionBuildsProfileFromTheKovaaksDirectory) {
        EXPECT_TRUE(app->profileService()->isProfileLoaded());
        EXPECT_FALSE(app->profileService()->getScenarioList().empty());
    }

    TEST_F(CompositionRootTest, SessionControllerReadsThroughToTheRealProfile) {
        // SessionController::getScenarioList delegates to the same wired ProfileService.
        EXPECT_EQ(app->sessionController()->getScenarioList().size(),
                  app->profileService()->getScenarioList().size());
    }

    TEST_F(CompositionRootTest, SetCurrentPerfResolvesThroughFileServiceAndDecoder) {
        const QString file = QDir(env.performancesDir()).absoluteFilePath("1wall6targets TE.perf");

        app->sessionController()->setCurrentPerf(file.toStdString());

        EXPECT_EQ(app->sessionController()->getCurrentRun().run_id.scenario_id.name, "1wall6targets TE");
    }

    TEST_F(CompositionRootTest, CurrentPerfChangeCascadesToGraphViewModelReload) {
        // App wires sessionController->currentRunChanged -> graphVm->fetchData() so the
        // graph reloads whenever currentPerf changes for any reason, not just file loads.
        const QString file = QDir(env.performancesDir()).absoluteFilePath("1wall6targets TE.perf");

        QSignalSpy spy(app->graphVm(), &presentation::GraphViewModelBase::dataUpdated);
        ASSERT_TRUE(spy.isValid());

        app->sessionController()->setCurrentPerf(file.toStdString());

        EXPECT_GE(spy.count(), 1);
    }

    TEST_F(CompositionRootTest, ProfileChangeCascadesToPlaytimeViewModelRefresh) {
        // The App wires m_profileService->onProfileChanged([]{ m_playtimeVm->refresh(); });
        // refresh() emits dataUpdated, so a profile mutation must reach the playtime VM.
        QSignalSpy spy(app->playtimeVm(), &presentation::GraphViewModelBase::dataUpdated);
        ASSERT_TRUE(spy.isValid());

        app->profileService()->generateProfileFromDirectory();

        EXPECT_GE(spy.count(), 1);
    }
}
