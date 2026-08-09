//
// Drives the real Main.qml scene graph with the real view models: opening the
// Settings dialog through the menu action, and populating the dashboard graph
// by loading a real .perf. These exercise the QML wiring/bindings that only
// exist at runtime -- the JS-fake ui_qml_tests can't load these components
// because they need the C++ GraphCanvas / GraphViewModel types.
//

#include <gtest/gtest.h>

#include <QQmlApplicationEngine>
#include <QRectF>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <memory>

#include "app/app.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/graph_vm.h"

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

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            const QString file = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());
            perfUrl = QUrl::fromLocalFile(file).toString();

            app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>());
            engine.setInitialProperties({
                {"graphVm", QVariant::fromValue(app->graphVm())},
                {"playtimeVm", QVariant::fromValue(app->playtimeVm())},
                {"sessionVm", QVariant::fromValue(app->sessionVm())},
                {"settingsVm", QVariant::fromValue(app->settingsVm())},
                {"scenarioBrowserVm", QVariant::fromValue(app->scenarioBrowserVm())},
            });
            engine.loadFromModule("KovaaksStatsViewer", "Main");
            ASSERT_FALSE(engine.rootObjects().isEmpty()) << "Main.qml failed to load";
            root = engine.rootObjects().first();
        }

        // The dashboard canvas is the GraphCanvas bound to the single-run graphVm
        // (Main.qml also has a second GraphCanvas for the playtime graph).
        [[nodiscard]] presentation::GraphCanvas *dashboardCanvas() const {
            for (auto *c: root->findChildren<presentation::GraphCanvas *>()) {
                if (c->graphVm() == app->graphVm()) return c;
            }
            return nullptr;
        }
    };

    TEST_F(DashboardUiTest, SettingsMenuActionOpensTheSettingsDialog) {
        auto *menuBar = root->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);

        // The dialog is created lazily by a Loader, so it must not exist yet.
        EXPECT_EQ(root->findChild<QObject *>("settingsDialog"), nullptr);

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

        // Same menu action must re-open the (Loader-cached) dialog.
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "settingsRequested"));
        EXPECT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
    }

    TEST_F(DashboardUiTest, DashboardShowsAllPlottableColumns) {
        auto *canvas = dashboardCanvas();
        ASSERT_NE(canvas, nullptr) << "no GraphCanvas bound to the single-run graphVm";

        // columnVisibilitySettings defaults every column to visible, so the
        // dashboard's computed visibleColumns must cover the full plottable set.
        EXPECT_EQ(canvas->visibleColumns().size(), app->graphVm()->plottableColumns().size());
        EXPECT_FALSE(canvas->visibleColumns().isEmpty());
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
}
