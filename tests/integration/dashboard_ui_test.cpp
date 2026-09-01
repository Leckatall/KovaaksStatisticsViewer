//
// Drives the real Main.qml scene graph with the real view models: opening the
// Settings dialog through the menu action, and populating the dashboard graph
// by loading a real .perf. These exercise the QML wiring/bindings that only
// exist at runtime -- the JS-fake ui_qml_tests can't load these components
// because they need the C++ GraphCanvas / GraphViewModel types.
//

#include <gtest/gtest.h>

#include <QDir>
#include <QQmlApplicationEngine>
#include <QRectF>
#include <QSettings>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>
#include <algorithm>
#include <filesystem>
#include <memory>

#include "app/app.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/graph_vm.h"
#include "settings_service.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    // Built-in SeriesId from defaultSeriesConfigs() -- GraphCanvas' y-axis column is
    // always a series' SeriesId, not a position.
    constexpr int kAccuracySeriesId = 2;

    // A temp KovaaKs dir with the fixture perf in it, a wired App, and the real
    // Main.qml scene loaded through App::start().
    struct DashboardScene {
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        QObject *root = nullptr;
        QString perfFile;
        QString perfUrl;

        // Returns the first thing that went wrong, or an empty string on success --
        // ASSERT_* cannot be used in a non-void function.
        [[nodiscard]] QString build() {
            if (!env.valid()) return "temp dir invalid";
            if (!env.makePerformancesDir()) return "could not create performances dir";
            perfFile = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            if (perfFile.isEmpty()) return "could not copy the perf fixture";
            perfUrl = QUrl::fromLocalFile(perfFile).toString();

            app = std::make_unique<application::App>(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
            if (app->start() != 0) return "Main.qml failed to load";
            root = app->engine()->rootObjects().first();
            return {};
        }

        [[nodiscard]] ui::GraphCanvas *canvasFor(const presentation::GraphViewModelBase *vm) const {
            for (auto *canvas: root->findChildren<ui::GraphCanvas *>()) {
                if (canvas->graphVm() == vm) return canvas;
            }
            return nullptr;
        }
    };

    // These tests share one App and one Main.qml scene: standing the whole thing up
    // per test dominated this suite's runtime, and none of them needs a pristine
    // profile. What they DO mutate is UI state, so SetUp() puts the dialog and the
    // visible-series set back before each one. Tests that need an untouched scene use
    // DashboardCleanSceneTest below instead.
    class DashboardUiTest : public testing::Test {
    protected:
        static std::unique_ptr<DashboardScene> scene;

        static void SetUpTestSuite() {
            scene = std::make_unique<DashboardScene>();
            const QString error = scene->build();
            ASSERT_TRUE(error.isEmpty()) << error.toStdString();
        }

        static void TearDownTestSuite() { scene.reset(); }

        void SetUp() override {
            if (auto *dialog = scene->root->findChild<QObject *>("settingsDialog")) {
                if (dialog->property("visible").toBool()) {
                    QMetaObject::invokeMethod(dialog, "close");
                    QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000);
                }
                dialog->setProperty("currentCategory", 0);
            }
            if (auto *visualSettings = scene->root->findChild<QObject *>("visualSettings")) {
                visualSettings->setProperty("visibleSeriesIds", scene->app->graphVm()->enabledSeriesIds());
            }
        }

        [[nodiscard]] static QObject *root() { return scene->root; }
        [[nodiscard]] static application::App *app() { return scene->app.get(); }
        [[nodiscard]] static QString perfUrl() { return scene->perfUrl; }
        [[nodiscard]] static ui::GraphCanvas *dashboardCanvas() {
            return scene->canvasFor(scene->app->graphVm());
        }
    };

    std::unique_ptr<DashboardScene> DashboardUiTest::scene;

    // For assertions that only hold on a scene nothing else has touched -- one asserts
    // the plot subtree has never been instantiated, the other replaces the profile.
    class DashboardCleanSceneTest : public testing::Test {
    protected:
        DashboardScene scene;

        void SetUp() override {
            const QString error = scene.build();
            ASSERT_TRUE(error.isEmpty()) << error.toStdString();
        }
    };

    TEST_F(DashboardUiTest, SettingsMenuActionOpensTheSettingsDialog) {
        auto *menuBar = root()->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);

        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));

        QObject *dialog = nullptr;
        const bool opened = QTest::qWaitFor([&] {
            dialog = root()->findChild<QObject *>("settingsDialog");
            return dialog != nullptr && dialog->property("visible").toBool();
        }, 3000);

        ASSERT_TRUE(opened) << "Settings dialog never opened after the Settings menu action";
        EXPECT_TRUE(dialog->property("visible").toBool());
    }

    TEST_F(DashboardUiTest, ReopeningSettingsDialogAfterCloseWorks) {
        auto *menuBar = root()->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);

        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));
        QObject *dialog = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            dialog = root()->findChild<QObject *>("settingsDialog");
            return dialog != nullptr && dialog->property("visible").toBool();
        }, 3000));

        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));

        // Same menu action must re-open the existing dialog instance.
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));
        EXPECT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
    }

    TEST_F(DashboardUiTest, DashboardFiltersEnabledSeriesThroughVisibleSettings) {
        app()->graphVm()->fetchData(perfUrl());

        auto *canvas = dashboardCanvas();
        ASSERT_NE(canvas, nullptr);
        QObject *visualSettings = root()->findChild<QObject *>("visualSettings");
        ASSERT_NE(visualSettings, nullptr);

        const auto enabledIds = app()->graphVm()->enabledSeriesIds();
        const auto selected = std::find_if(enabledIds.cbegin(), enabledIds.cend(), [](const QVariant &id) {
            return app()->graphVm()->columnForSeriesId(id.toString()) != -1;
        });
        ASSERT_NE(selected, enabledIds.cend());
        const int selectedColumn = app()->graphVm()->columnForSeriesId(selected->toString());

        visualSettings->setProperty("visibleSeriesIds", QVariantList{*selected});

        ASSERT_TRUE(QTest::qWaitFor([&] {
            return canvas->visibleColumns() == QVariantList{selectedColumn};
        }, 2000));
    }

    TEST_F(DashboardUiTest, ConfigureActionsOpenGraphLinesSettingsCategory) {
        auto *menuBar = root()->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "configureGraphLinesRequested"));

        auto *dialog = root()->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentCategory").toInt() == 1;
        }, 3000));

        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));
        dialog->setProperty("currentCategory", 0);

        auto *button = root()->findChild<QObject *>("configureGraphLinesButton");
        ASSERT_NE(button, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(button, "clicked"));
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentCategory").toInt() == 1;
        }, 3000));
    }

    TEST_F(DashboardUiTest, FetchingPerfPopulatesDashboardTitleAndGraph) {
        app()->graphVm()->fetchData(perfUrl());

        auto *titleLabel = root()->findChild<QObject *>("scenarioTitleLabel");
        ASSERT_NE(titleLabel, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return titleLabel->property("text").toString().contains("1wall6targets TE");
        }, 2000)) << "scenario title label never reflected the loaded perf";

        auto *canvas = dashboardCanvas();
        ASSERT_NE(canvas, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return canvas->property("plotArea").toRectF().width() > 0.0;
        }, 2000)) << "dashboard canvas never got a laid-out plot area";

        // valuesAtX only reports valid when a series actually has data plotted,
        // so this confirms the perf populated the graph end-to-end through QML.
        const QRectF plot = canvas->property("plotArea").toRectF();
        const QVariantMap sample = canvas->valuesAtX(plot.center().x());
        EXPECT_TRUE(sample.value("valid").toBool()) << "dashboard graph has no plotted data at mid-x";
    }

    TEST_F(DashboardUiTest, YAxisTitleLabelDefaultsToScore) {
        // The plot subtree (and its axis title) is only instantiated once a run with performance data
        // is shown; before that the panel carries the "No run selected" message instead.
        app()->graphVm()->fetchData(perfUrl());

        QObject *label = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            label = root()->findChild<QObject *>("scenarioYAxisTitleLabel");
            return label != nullptr;
        }, 2000)) << "no label named 'scenarioYAxisTitleLabel' found after loading a perf";
        const auto series = app()->graphVm()->series({app()->graphVm()->yAxisColumn()});
        ASSERT_FALSE(series.isEmpty());
        EXPECT_EQ(label->property("text").toString(), series.front()->name());
    }

    TEST_F(DashboardCleanSceneTest, NoRunSelectedMessageShowsBeforeAnyRunIsLoaded) {
        auto *label = scene.root->findChild<QObject *>("graphNoRunSelectedLabel");
        ASSERT_NE(label, nullptr) << "no label named 'graphNoRunSelectedLabel' found in the dashboard scene";
        EXPECT_TRUE(label->property("visible").toBool());
        EXPECT_EQ(scene.root->findChild<QObject *>("scenarioYAxisTitleLabel"), nullptr)
            << "plot should not be instantiated while no run is shown";
    }

    // Its own scene: this is the one test that repoints the canvas' y-axis column, and
    // YAxisTitleLabelDefaultsToScore reads that same state.
    TEST_F(DashboardCleanSceneTest, YAxisTitleLabelTracksTheSelectedColumn) {
        auto *app = scene.app.get();
        app->graphVm()->fetchData(scene.perfUrl);

        auto *canvas = scene.canvasFor(app->graphVm());
        ASSERT_NE(canvas, nullptr);
        QObject *label = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            label = scene.root->findChild<QObject *>("scenarioYAxisTitleLabel");
            return label != nullptr;
        }, 2000)) << "no label named 'scenarioYAxisTitleLabel' found after loading a perf";

        canvas->setYAxisColumn(kAccuracySeriesId);

        const auto accuracy = app->graphVm()->series({kAccuracySeriesId});
        ASSERT_FALSE(accuracy.isEmpty());
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return label->property("text").toString() == accuracy.front()->name();
        }, 2000)) << "axis title stayed on '" << label->property("text").toString().toStdString()
                  << "' after the y-axis column moved to " << accuracy.front()->name().toStdString();
    }

    TEST_F(DashboardCleanSceneTest, ScenarioHistoryCanvasUsesTheCompletionHistoryViewModel) {
        auto *app = scene.app.get();
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));

        // Take the run the live pipeline already decoded rather than standing up a
        // second ProtoDecoder over the same file.
        auto first_run = app->profileService()->getLatestRun();
        ASSERT_EQ(first_run.run_id.scenario_id.name, "1wall6targets TE");
        auto second_run = first_run;
        second_run.run_id.start_time += 1000;
        domain::UserProfile profile;
        const auto source = profile.ensureSource(scene.env.rootPath().toStdString(), "FPSAimTrainer/performances");
        first_run.sources.perf = {{source, std::filesystem::path(scene.perfFile.toStdString()).filename().string()}};
        second_run.sources.perf = first_run.sources.perf;
        ASSERT_TRUE(profile.addRun(first_run));
        ASSERT_TRUE(profile.addRun(second_run));

        app->profileService()->applyBuiltProfile(std::move(profile));

        const auto historyCanvas = [this, app] { return scene.canvasFor(app->completionHistoryVm()); };
        ASSERT_TRUE(QTest::qWaitFor([&] { return historyCanvas() != nullptr; }, 3000));
        EXPECT_EQ(historyCanvas()->graphVm(), app->completionHistoryVm());
    }

    TEST(FirstRunUiTest, BannerOpensFolderDialogAndHidesAfterDirectorySelection) {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.remove("file/kovaaks");
        raw.remove("file/kovaaksDirs");
        raw.sync();

        QTemporaryDir tempDir;
        ASSERT_TRUE(tempDir.isValid());
        const auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
        settings->setProfilePath(QDir(tempDir.path()).filePath("profile.pb").toStdString());
        const auto seriesConfigStore = std::make_shared<qt_data::SeriesConfigStore>(settings);
        application::App app(settings, std::make_shared<data::ProtoDecoder>(), seriesConfigStore);
        ASSERT_EQ(app.start(), 0) << "Main.qml failed to load";

        auto *root = app.engine()->rootObjects().first();
        auto *banner = root->findChild<QObject *>("firstRunBanner");
        auto *dialog = root->findChild<QObject *>("kovaaksFolderDialog");
        ASSERT_NE(banner, nullptr);
        ASSERT_NE(dialog, nullptr);
        EXPECT_TRUE(banner->property("visible").toBool());

        ASSERT_TRUE(QMetaObject::invokeMethod(banner, "chooseFolderRequested"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
        dialog->setProperty("visible", false);

        app.settingsVm()->setKovaaksDir(QUrl::fromLocalFile("D:/Kovaaks"));

        EXPECT_TRUE(QTest::qWaitFor([&] { return !banner->property("visible").toBool(); }, 3000));
    }
}
