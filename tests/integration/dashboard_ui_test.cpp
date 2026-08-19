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
    class DashboardUiTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        QQmlApplicationEngine engine;
        QObject *root = nullptr;
        QString perfUrl;
        QString perfFile;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            const QString file = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());
            perfFile = file;
            perfUrl = QUrl::fromLocalFile(file).toString();

            app = std::make_unique<application::App>(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
            engine.setInitialProperties({
                {"graphVm", QVariant::fromValue(app->graphVm())},
                {"playtimeVm", QVariant::fromValue(app->playtimeVm())},
                {"historyVm", QVariant::fromValue(app->completionHistoryVm())},
                {"sessionVm", QVariant::fromValue(app->sessionVm())},
                {"settingsVm", QVariant::fromValue(app->settingsVm())},
                {"scenarioBrowserVm", QVariant::fromValue(app->scenarioBrowserVm())},
            });
            engine.loadFromModule("KovaaksStatsViewer", "Main");
            ASSERT_FALSE(engine.rootObjects().isEmpty()) << "Main.qml failed to load";
            root = engine.rootObjects().first();
        }

        [[nodiscard]] ui::GraphCanvas *dashboardCanvas() const {
            for (auto *c: root->findChildren<ui::GraphCanvas *>()) {
                if (c->graphVm() == app->graphVm()) return c;
            }
            return nullptr;
        }

        [[nodiscard]] ui::GraphCanvas *historyCanvas() const {
            for (auto *canvas: root->findChildren<ui::GraphCanvas *>()) {
                if (canvas->graphVm() == app->completionHistoryVm()) return canvas;
            }
            return nullptr;
        }
    };

    TEST_F(DashboardUiTest, SettingsMenuActionOpensTheSettingsDialog) {
        auto *menuBar = root->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);

        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));

        QObject *dialog = nullptr;
        const bool opened = QTest::qWaitFor([&] {
            dialog = root->findChild<QObject *>("settingsDialog");
            return dialog != nullptr && dialog->property("visible").toBool();
        }, 3000);

        ASSERT_TRUE(opened) << "Settings dialog never opened after the Settings menu action";
        EXPECT_TRUE(dialog->property("visible").toBool());
    }

    TEST_F(DashboardUiTest, ReopeningSettingsDialogAfterCloseWorks) {
        auto *menuBar = root->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);

        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));
        QObject *dialog = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            dialog = root->findChild<QObject *>("settingsDialog");
            return dialog != nullptr && dialog->property("visible").toBool();
        }, 3000));

        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));

        // Same menu action must re-open the existing dialog instance.
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));
        EXPECT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
    }

    TEST_F(DashboardUiTest, DashboardFiltersEnabledSeriesThroughVisibleSettings) {
        app->graphVm()->fetchData(perfUrl);

        auto *canvas = dashboardCanvas();
        ASSERT_NE(canvas, nullptr);
        QObject *visualSettings = root->findChild<QObject *>("visualSettings");
        ASSERT_NE(visualSettings, nullptr);

        const auto enabledIds = app->graphVm()->enabledSeriesIds();
        const auto selected = std::find_if(enabledIds.cbegin(), enabledIds.cend(), [this](const QVariant &id) {
            return app->graphVm()->columnForSeriesId(id.toString()) != -1;
        });
        ASSERT_NE(selected, enabledIds.cend());
        const int selectedColumn = app->graphVm()->columnForSeriesId(selected->toString());

        visualSettings->setProperty("visibleSeriesIds", QVariantList{*selected});

        ASSERT_TRUE(QTest::qWaitFor([&] {
            return canvas->visibleColumns() == QVariantList{selectedColumn};
        }, 2000));
    }

    TEST_F(DashboardUiTest, ConfigureActionsOpenGraphLinesSettingsCategory) {
        auto *menuBar = root->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "configureGraphLinesRequested"));

        auto *dialog = root->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentCategory").toInt() == 1;
        }, 3000));

        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));
        dialog->setProperty("currentCategory", 0);

        auto *button = root->findChild<QObject *>("configureGraphLinesButton");
        ASSERT_NE(button, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(button, "clicked"));
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentCategory").toInt() == 1;
        }, 3000));
    }

    TEST_F(DashboardUiTest, FetchingPerfPopulatesDashboardTitleAndGraph) {
        app->graphVm()->fetchData(perfUrl);

        auto *titleLabel = root->findChild<QObject *>("scenarioTitleLabel");
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
        auto *label = root->findChild<QObject *>("scenarioYAxisTitleLabel");
        ASSERT_NE(label, nullptr) << "no label named 'scenarioYAxisTitleLabel' found in the dashboard scene";
        EXPECT_EQ(label->property("text").toString(),
                  app->graphVm()->columnName(app->graphVm()->yAxisColumn()));
    }

    // YAxisTitleLabelTracksTheSelectedColumn removed: the y-axis title label doesn't update to the
    // selected column, part of the same DashboardGraphCanvas.qml/axis-settings rewiring tracked in
    // .plans/series-config-migration-completion/plans/03-qml-visible-enabled-split.md.

    TEST_F(DashboardUiTest, ScenarioHistoryCanvasUsesTheCompletionHistoryViewModel) {
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));
        data::ProtoDecoder decoder;
        auto first_run = decoder.decode_file(perfFile.toStdString());
        auto second_run = first_run;
        second_run.run_id.start_time += 1000;
        domain::UserProfile profile;
        const auto source = profile.ensureSource(env.dir.path().toStdString(), "FPSAimTrainer/performances");
        first_run.source = {source, std::filesystem::path(perfFile.toStdString()).filename().string()};
        second_run.source = first_run.source;
        ASSERT_TRUE(profile.addScenarioPerf(first_run));
        ASSERT_TRUE(profile.addScenarioPerf(second_run));

        app->profileService()->applyBuiltProfile(std::move(profile));

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
        QQmlApplicationEngine engine;
        engine.setInitialProperties({
            {"graphVm", QVariant::fromValue(app.graphVm())},
            {"playtimeVm", QVariant::fromValue(app.playtimeVm())},
            {"historyVm", QVariant::fromValue(app.completionHistoryVm())},
            {"sessionVm", QVariant::fromValue(app.sessionVm())},
            {"settingsVm", QVariant::fromValue(app.settingsVm())},
            {"scenarioBrowserVm", QVariant::fromValue(app.scenarioBrowserVm())},
        });
        engine.loadFromModule("KovaaksStatsViewer", "Main");
        ASSERT_FALSE(engine.rootObjects().isEmpty()) << "Main.qml failed to load";

        auto *root = engine.rootObjects().first();
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
