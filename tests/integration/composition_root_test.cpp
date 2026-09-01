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
    // One App for the whole suite. Building it runs loadProfile(), which asks
    // SessionController for a build on its worker thread when no store is available,
    // and the result only lands once the event loop spins -- a wait every test used to
    // repeat for a structurally identical graph. The tests that mutate the wired state
    // (current run, profile) each re-establish their own starting point rather than
    // relying on a fresh graph.
    class CompositionRootTest : public testing::Test {
    protected:
        static std::unique_ptr<integration::TestEnv> env;
        static std::unique_ptr<application::App> app;

        static void SetUpTestSuite() {
            env = std::make_unique<integration::TestEnv>();
            ASSERT_TRUE(env->valid());
            ASSERT_TRUE(env->makePerformancesDir());
            ASSERT_FALSE(env->copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            ASSERT_FALSE(env->copyFixtureIntoPerformances("VT FlyTS Novice S5.perf").isEmpty());

            app = std::make_unique<application::App>(
                env->settings, std::make_shared<data::ProtoDecoder>(), env->seriesConfigStore);
            ASSERT_TRUE(waitForProfile());
        }

        // App holds shared_ptrs to services built over env's settings, so it goes first.
        static void TearDownTestSuite() {
            app.reset();
            env.reset();
        }

        [[nodiscard]] static bool waitForProfile() {
            QElapsedTimer timer;
            timer.start();
            while (!app->profileService()->isProfileLoaded()) {
                if (timer.elapsed() > 5000) return false;
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
            return true;
        }

        [[nodiscard]] static QString fixture(const QString &name) {
            return QDir(env->performancesDir()).absoluteFilePath(name);
        }
    };

    std::unique_ptr<integration::TestEnv> CompositionRootTest::env;
    std::unique_ptr<application::App> CompositionRootTest::app;

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
        app->sessionController()->setCurrentPerf(fixture("1wall6targets TE.perf").toStdString());

        EXPECT_EQ(app->sessionController()->getCurrentRun().run_id.scenario_id.name, "1wall6targets TE");
    }

    TEST_F(CompositionRootTest, CurrentPerfChangeCascadesToGraphViewModelReload) {
        // App wires sessionController->currentRunChanged -> graphVm->fetchData() so the
        // graph reloads whenever currentPerf changes for any reason, not just file loads.
        // setCurrentRun ignores a run_id it is already on, and this suite shares one App,
        // so park on the other fixture first to guarantee the call below is a real change.
        app->sessionController()->setCurrentPerf(fixture("VT FlyTS Novice S5.perf").toStdString());

        QSignalSpy spy(app->graphVm(), &presentation::GraphViewModelBase::dataUpdated);
        ASSERT_TRUE(spy.isValid());

        app->sessionController()->setCurrentPerf(fixture("1wall6targets TE.perf").toStdString());

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
