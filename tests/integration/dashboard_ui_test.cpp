//
// Drives the real Main.qml scene graph with the real view models: opening the
// Settings dialog through the menu action, and populating the dashboard graph
// by loading a real .perf. These exercise the QML wiring/bindings that only
// exist at runtime -- the JS-fake ui_qml_tests can't load these components
// because they need the C++ GraphCanvas / GraphViewModel types.
//

#include <gtest/gtest.h>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QRect>
#include <QRectF>
#include <QSettings>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>

#include "app/app.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_history_vm.h"
#include "presentation/benchmark_tracking_vm.h"
#include "presentation/completion_history_vm.h"
#include "presentation/graph_vm.h"
#include "settings_service.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    // Built-in SeriesId from defaultSeriesConfigs() -- GraphCanvas' y-axis column is
    // always a series' SeriesId, not a position.
    constexpr int kAccuracySeriesId = 2;

    [[nodiscard]] int countPixelsNear(const QImage &image, const QRect &area, const QColor &colour) {
        constexpr int kTolerance = 8;
        int count = 0;
        const QRect clipped = area.intersected(image.rect());
        for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
            for (int x = clipped.left(); x <= clipped.right(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.alpha() > 200 && std::abs(pixel.red() - colour.red()) <= kTolerance &&
                    std::abs(pixel.green() - colour.green()) <= kTolerance &&
                    std::abs(pixel.blue() - colour.blue()) <= kTolerance)
                    ++count;
            }
        }
        return count;
    }

    [[nodiscard]] QQuickItem *findItem(QQuickItem *parent, const QString &name) {
        if (!parent) return nullptr;
        if (parent->objectName() == name) return parent;
        for (auto *child: parent->childItems())
            if (auto *found = findItem(child, name)) return found;
        return nullptr;
    }

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
                env.settings, std::make_shared<data::ProtoDecoder>(), env.graphConfigurationPersistence,
                env.statsParser, env.benchmarkStore, env.daySource);
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

    // Hover dots stack like the lines: the dot of the series higher in the line list is on top,
    // while the tooltip keeps listing the series top-of-list first.
    TEST_F(DashboardUiTest, HoverDotsStackInLineListOrder) {
        app()->graphVm()->fetchData(perfUrl());

        auto *canvas = dashboardCanvas();
        ASSERT_NE(canvas, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return canvas->property("plotArea").toRectF().width() > 0.0 && canvas->visibleColumns().size() >= 2;
        }, 2000)) << "dashboard canvas never showed two or more lines";
        // The hover area and the dots are siblings of the canvas inside GraphCanvasWithTooltip.
        QQuickItem *tooltipRoot = canvas->parentItem();
        ASSERT_NE(tooltipRoot, nullptr);
        const auto siblingsNamed = [tooltipRoot](const QString &name) {
            QList<QQuickItem *> found;
            for (auto *child: tooltipRoot->childItems()) if (child->objectName() == name) found.append(child);
            return found;
        };
        const auto hoverAreas = siblingsNamed("graphHoverArea");
        ASSERT_EQ(hoverAreas.size(), 1);
        QQuickItem *hoverArea = hoverAreas.front();

        const QVariantMap info = canvas->valuesAtX(canvas->property("plotArea").toRectF().center().x());
        ASSERT_TRUE(info.value("valid").toBool());
        hoverArea->setProperty("hoverInfo", info);

        const QVariantList listed = info.value("series").toList();
        QList<QQuickItem *> dots;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            dots = siblingsNamed("graphHoverDot");
            return dots.size() == listed.size();
        }, 2000)) << "expected one hover dot per listed series";
        std::ranges::sort(dots, [](const QQuickItem *a, const QQuickItem *b) { return a->z() > b->z(); });
        for (int i = 0; i < dots.size(); ++i) {
            EXPECT_EQ(dots[i]->property("color").value<QColor>(), listed[i].toMap().value("color").value<QColor>())
                << "dot " << i << " from the top is not the " << i << "-th listed series";
        }

        hoverArea->setProperty("hoverInfo", QVariantMap{{"valid", false}});
    }

    // The three Scenarios-tab graph panels split the column equally, whatever their content, and
    // the visible ones still do when one is hidden.
    TEST_F(DashboardUiTest, GraphPanelsShareTheColumnEqually) {
        app()->graphVm()->fetchData(perfUrl());

        auto *run = root()->findChild<QObject *>("scenarioGraphPanel");
        auto *playtime = root()->findChild<QObject *>("playtimeGraphPanel");
        auto *history = root()->findChild<QObject *>("scenarioHistoryPanel");
        QObject *visualSettings = root()->findChild<QObject *>("visualSettings");
        ASSERT_TRUE(run && playtime && history && visualSettings);
        const auto height = [](const QObject *item) { return item->property("height").toReal(); };

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return height(run) > 0.0 && std::abs(height(run) - height(playtime)) <= 1.0 &&
                   std::abs(height(run) - height(history)) <= 1.0;
        }, 2000)) << "panel heights " << height(run) << ", " << height(playtime) << ", " << height(history);

        visualSettings->setProperty("playtimeGraphVisible", false);
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return height(run) > 0.0 && std::abs(height(run) - height(history)) <= 1.0 &&
                   height(run) > height(playtime);
        }, 2000)) << "with playtime hidden: " << height(run) << ", " << height(history);
        visualSettings->setProperty("playtimeGraphVisible", true);
    }

    // The panels about the selected scenario (run graph, its history) sit together, above the
    // all-scenario playtime panel.
    TEST_F(DashboardUiTest, GraphPanelsAreOrderedRunHistoryPlaytime) {
        auto *run = root()->findChild<QObject *>("scenarioGraphPanel");
        auto *history = root()->findChild<QObject *>("scenarioHistoryPanel");
        auto *playtime = root()->findChild<QObject *>("playtimeGraphPanel");
        ASSERT_TRUE(run && history && playtime);
        const auto y = [](const QObject *item) { return item->property("y").toReal(); };

        EXPECT_TRUE(QTest::qWaitFor([&] { return y(run) < y(history) && y(history) < y(playtime); }, 2000))
            << "panel y: run " << y(run) << ", history " << y(history) << ", playtime " << y(playtime);
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

    // The popup edits the committed configuration; More graph settings then opens the settings
    // visit on that graph, seeded with the change, without leaving anything pending.
    TEST_F(DashboardCleanSceneTest, TheGearPopupHandsItsQuickChangesToTheGraphsSettingsPage) {
        auto *gear = scene.root->findChild<QQuickItem *>("graphGear_dailyPlaytime");
        ASSERT_NE(gear, nullptr) << "no gear on the daily playtime graph, which has no data in this scene";
        ASSERT_TRUE(QMetaObject::invokeMethod(gear, "clicked"));
        auto *popup = gear->property("popup").value<QObject *>();
        ASSERT_NE(popup, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] { return popup->property("opened").toBool(); }, 3000));

        auto *settings = scene.app->settingsVm();
        const auto quick = settings->applyQuickPatch("dailyPlaytime", {{"window", 7}});
        ASSERT_TRUE(quick["accepted"].toBool());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), 7);

        auto *content = popup->property("contentItem").value<QQuickItem *>();
        auto *windowField = findItem(content, "graphField_dataRange_averagingWindow");
        ASSERT_NE(windowField, nullptr);
        EXPECT_EQ(windowField->property("text").toString(), "7");
        auto *more = findItem(content, "graphMoreSettings_dailyPlaytime");
        ASSERT_NE(more, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(more, "clicked"));

        auto *dialog = scene.root->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentCategory").toInt() == 2;
        }, 3000));
        EXPECT_FALSE(popup->property("opened").toBool());
        EXPECT_EQ(settings->selectedGraph(), "dailyPlaytime");
        EXPECT_TRUE(settings->isVisitOpen());
        EXPECT_FALSE(settings->hasPendingChanges());
        auto *selector = findItem(dialog->property("contentItem").value<QQuickItem *>(), "graphSelector");
        ASSERT_NE(selector, nullptr);
        EXPECT_EQ(selector->property("currentIndex").toInt(), 1);
    }

    // Typing into the real Graphs page edits the visit's working copy only; Save commits it, and an
    // unparseable entry stays beside its control, blocks Save and leaves the dashboard as it was.
    TEST_F(DashboardCleanSceneTest, TypingIntoTheGraphsPageStagesUntilSaveAndABadEntryBlocksIt) {
        auto *settings = scene.app->settingsVm();
        auto *dialog = scene.root->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "openGraph", Q_ARG(QVariant, QVariant("dailyPlaytime"))));
        ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
        auto *content = dialog->property("contentItem").value<QQuickItem *>();
        auto *footer = dialog->property("footer").value<QQuickItem *>();
        auto *windowField = findItem(content, "graphField_dataRange_averagingWindow");
        auto *error = findItem(content, "graphField_dataRange_averagingWindow_error");
        auto *save = findItem(footer, "settingsSaveButton");
        ASSERT_NE(windowField, nullptr);
        ASSERT_NE(error, nullptr);
        ASSERT_NE(save, nullptr);
        const int committed = scene.app->playtimeVm()->averagingWindowDays();
        const auto type = [&](const QString &text) {
            windowField->setProperty("text", text);
            QMetaObject::invokeMethod(windowField, "textEdited");
        };

        type("9");
        EXPECT_TRUE(settings->hasPendingChanges());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), committed) << "a draft edit must not reach the graph";
        EXPECT_FALSE(error->property("visible").toBool());

        type("9x");
        EXPECT_TRUE(error->property("visible").toBool());
        EXPECT_FALSE(error->property("text").toString().isEmpty());
        ASSERT_TRUE(QMetaObject::invokeMethod(save, "clicked"));
        EXPECT_TRUE(settings->isVisitOpen());
        EXPECT_FALSE(settings->issues().isEmpty());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), committed);

        type("9");
        EXPECT_FALSE(error->property("visible").toBool());
        ASSERT_TRUE(QMetaObject::invokeMethod(save, "clicked"));
        EXPECT_FALSE(settings->hasPendingChanges());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), 9);
    }

    // Closing the real settings window over a staged edit asks first; cancelling the close keeps the
    // window, the draft and the dashboard as they were, and discarding drops the draft.
    TEST_F(DashboardCleanSceneTest, ClosingADirtySettingsWindowKeepsTheDraftUntilTheUserDecides) {
        auto *settings = scene.app->settingsVm();
        auto *dialog = scene.root->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "openGraph", Q_ARG(QVariant, QVariant("dailyPlaytime"))));
        ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
        auto *prompt = dialog->property("discardChangesPrompt").value<QObject *>();
        ASSERT_NE(prompt, nullptr);
        const int committed = scene.app->playtimeVm()->averagingWindowDays();
        const int staged = committed == 9 ? 11 : 9;
        ASSERT_TRUE(settings->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", QString::number(staged)));
        ASSERT_TRUE(settings->hasPendingChanges());

        // QPlatformDialogHelper::Cancel / Discard with RejectRole / DestructiveRole.
        const auto click = [&](const int button, const int role) {
            QQmlExpression click(QQmlEngine::contextForObject(prompt), prompt,
                                 QString("buttonClicked(%1, %2)").arg(button).arg(role));
            click.evaluate();
            ASSERT_FALSE(click.hasError()) << click.error().toString().toStdString();
        };
        const auto closeWindow = [&] {
            ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
            ASSERT_TRUE(QTest::qWaitFor([&] { return prompt->property("visible").toBool(); }, 3000));
        };

        ASSERT_NO_FATAL_FAILURE(closeWindow());
        EXPECT_TRUE(dialog->property("visible").toBool());
        EXPECT_TRUE(settings->hasPendingChanges());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), committed);

        ASSERT_NO_FATAL_FAILURE(click(0x00400000, 1));
        EXPECT_TRUE(dialog->property("visible").toBool()) << "Cancel keeps the window open";
        EXPECT_TRUE(settings->isVisitOpen());
        EXPECT_TRUE(settings->hasPendingChanges());
        EXPECT_EQ(settings->graphDraft("dailyPlaytime")["window"].toInt(), staged);
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), committed);

        ASSERT_NO_FATAL_FAILURE(closeWindow());
        ASSERT_NO_FATAL_FAILURE(click(0x00800000, 2));
        EXPECT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));
        EXPECT_FALSE(settings->isVisitOpen());
        EXPECT_FALSE(settings->hasPendingChanges());
        EXPECT_EQ(settings->graphDraft("dailyPlaytime")["window"].toInt(), committed);
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), committed);
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

    // A click on the history graph selects the nearest run: the history marks it, the run list's
    // current item follows it, and the run graph shows it. A click outside the plot does nothing.
    TEST_F(DashboardCleanSceneTest, ClickingTheHistoryGraphSelectsTheNearestRun) {
        auto *app = scene.app.get();
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));

        auto base_run = app->profileService()->getLatestRun();
        domain::UserProfile profile;
        const auto source = profile.ensureSource(scene.env.rootPath().toStdString(), "FPSAimTrainer/performances");
        base_run.sources.perf = {{source, std::filesystem::path(scene.perfFile.toStdString()).filename().string()}};
        for (int i = 0; i < 3; ++i) {
            auto run = base_run;
            run.run_id.start_time += 1000 * i;
            ASSERT_TRUE(profile.addRun(run));
        }
        app->profileService()->applyBuiltProfile(std::move(profile));

        auto *historyVm = app->completionHistoryVm();
        ui::GraphCanvas *canvas = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            canvas = scene.canvasFor(historyVm);
            return canvas != nullptr && historyVm->runCount() == 3 && canvas->width() > 0;
        }, 5000));
        app->completionHistoryVm()->selectRun(3);
        ASSERT_TRUE(QTest::qWaitFor([&] { return historyVm->selectedRunIndex() == 3; }, 2000));

        QQuickItem *runList = nullptr;
        for (auto *item: scene.root->findChildren<QQuickItem *>("runListView"))
            if (item->property("model").value<QObject *>() == app->scenarioBrowserVm()->runModel()) runList = item;
        ASSERT_NE(runList, nullptr);

        const QRectF plot = canvas->property("plotArea").toRectF();
        const auto clickAt = [&](const QPointF &local) {
            QTest::mouseClick(canvas->window(), Qt::LeftButton, {}, canvas->mapToScene(local).toPoint());
        };

        // Nearer run #1 than #2: x a fifth of the way across a plot spanning runs 1..3.
        clickAt({plot.left() + plot.width() * 0.2, plot.center().y()});
        EXPECT_TRUE(QTest::qWaitFor([&] { return historyVm->selectedRunIndex() == 1; }, 2000))
            << "selected run index " << historyVm->selectedRunIndex();
        EXPECT_EQ(canvas->markedX(), 1.0);
        EXPECT_EQ(app->scenarioBrowserVm()->currentRunStartTimeMs(), static_cast<double>(base_run.run_id.start_time));
        // The run list is newest first, so run #1 of 3 is its last row.
        EXPECT_TRUE(QTest::qWaitFor([&] { return runList->property("currentIndex").toInt() == 2; }, 2000))
            << "run list current index " << runList->property("currentIndex").toInt();

        // Left of the plot, on the y-axis labels: nothing changes.
        QTest::qWait(500);
        clickAt({plot.left() / 2, plot.center().y()});
        QTest::qWait(100);
        EXPECT_EQ(historyVm->selectedRunIndex(), 1);
    }

    // The wired history graph and the live QML canvas honour a numeric axis policy: fixed bounds are
    // exact displayed values and what they exclude is clipped, not painted past the plot.
    TEST_F(DashboardCleanSceneTest, ScenarioHistoryFixedBoundsReachTheLiveCanvasAndClipExcludedData) {
        auto *app = scene.app.get();
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));

        auto base_run = app->profileService()->getLatestRun();
        domain::UserProfile profile;
        const auto source = profile.ensureSource(scene.env.rootPath().toStdString(), "FPSAimTrainer/performances");
        base_run.sources.perf = {{source, std::filesystem::path(scene.perfFile.toStdString()).filename().string()}};
        for (int i = 0; i < 3; ++i) {
            auto run = base_run;
            run.run_id.start_time += 1000 * i;
            ASSERT_TRUE(profile.addRun(run));
        }
        app->profileService()->applyBuiltProfile(std::move(profile));

        auto *historyVm = app->completionHistoryVm();
        ui::GraphCanvas *canvas = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            canvas = scene.canvasFor(historyVm);
            return canvas != nullptr && historyVm->runCount() == 3 && canvas->width() > 0 && canvas->height() > 0;
        }, 5000));
        const auto score = historyVm->series({presentation::CompletionHistoryViewModel::Score});
        ASSERT_EQ(score.size(), 1);
        const QColor scoreColour = score.front()->color();
        const qreal runScore = score.front()->points.front().y();
        const auto render = [&] {
            QImage image(canvas->size().toSize(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            canvas->paint(&painter);
            return image;
        };

        // Every run scores the same, so fitting draws one flat line inside the plot.
        const QRect plot = canvas->property("plotArea").toRectF().toAlignedRect();
        EXPECT_GT(countPixelsNear(render(), plot, scoreColour), 0) << "the score line is drawn under the default fit";

        historyVm->setAxisPolicies({{"score", application::FixedBounds{runScore + 100.0, runScore + 200.0}}});
        const auto fixedSeries = historyVm->series({presentation::CompletionHistoryViewModel::Score});
        ASSERT_TRUE(fixedSeries.front()->yAxis.has_value());
        EXPECT_DOUBLE_EQ(fixedSeries.front()->yAxis->min(), runScore + 100.0);
        EXPECT_DOUBLE_EQ(fixedSeries.front()->yAxis->max(), runScore + 200.0);
        const QImage image = render();
        EXPECT_EQ(countPixelsNear(image, plot.adjusted(0, -40, 0, 40), scoreColour), 0)
            << "the line lies below the floor, so none of it may be painted";

        historyVm->setAxisPolicies({});
        EXPECT_GT(countPixelsNear(render(), plot, scoreColour), 0) << "dropping the policy restores the fitted line";
    }

    // Regression: BenchmarkHistoryCard never handed its canvas any visibleColumns, so
    // series([]) came back empty and nothing was drawn -- yet both axes still painted,
    // because the x axis is the VM's and the y axis falls back through labelledYAxisColumn()
    // without the column filter. Scanning pixels rather than the canvas' inputs catches any
    // other "axes but no line" cause too.
    TEST(BenchmarkHistoryUiTest, BothHistoryCanvasesPaintTheirSeries) {
        DashboardScene scene;
        {
            QFile file(QDir(scene.env.benchmarksDir.path()).absoluteFilePath("auto.json"));
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(R"({
                "schemaVersion": 1, "id": "auto-1", "name": "Auto",
                "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
                "uncategorized": [ { "id": "s1", "name": "1wall6targets TE", "hash": null,
                    "thresholds": [ { "tierId": "t1", "score": 1000000000.0 } ] } ],
                "categories": []
            })");
        }
        const QString error = scene.build();
        ASSERT_TRUE(error.isEmpty()) << error.toStdString();
        auto *app = scene.app.get();
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));

        // The tier is out of reach, so the rank line stays below Bronze's tier line, which is
        // painted over the series and would hide a line running exactly along it.
        // Average-rank history only gains a point on a day the personal best improves.
        auto first_run = app->profileService()->getLatestRun();
        auto second_run = first_run;
        second_run.run_id.start_time += 2LL * 24 * 60 * 60 * 1000;
        second_run.stored_totals.score = first_run.totals().score + 50.0F;
        domain::UserProfile profile;
        const auto source = profile.ensureSource(scene.env.rootPath().toStdString(), "FPSAimTrainer/performances");
        first_run.sources.perf = {{source, std::filesystem::path(scene.perfFile.toStdString()).filename().string()}};
        second_run.sources.perf = first_run.sources.perf;
        ASSERT_TRUE(profile.addRun(first_run));
        ASSERT_TRUE(profile.addRun(second_run));
        app->profileService()->applyBuiltProfile(std::move(profile));

        auto *tracking = app->benchmarkTrackingVm();
        tracking->selectBenchmark("auto-1");
        auto *tabBar = scene.root->findChild<QObject *>("workspaceTabBar");
        ASSERT_NE(tabBar, nullptr);
        tabBar->setProperty("currentIndex", 1);
        ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "trackable"; }, 3000))
            << "state: " << tracking->state().toStdString();

        for (auto *vm: {tracking->rankHistory(), tracking->playtimeHistory()}) {
            const std::string metric = vm->metricName().toStdString();
            ASSERT_TRUE(vm->hasData()) << metric;
            const auto series = vm->series({presentation::BenchmarkHistoryViewModel::Value});
            ASSERT_EQ(series.size(), 1) << metric;
            ASSERT_GE(series.front()->displayPoints().size(), 2) << metric;

            ui::GraphCanvas *canvas = nullptr;
            ASSERT_TRUE(QTest::qWaitFor([&] {
                canvas = scene.canvasFor(vm);
                return canvas && canvas->width() > 0 && canvas->height() > 0;
            }, 3000)) << metric << ": no laid-out canvas";

            QImage image(canvas->size().toSize(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                canvas->paint(&painter);
            }
            const QRect plot = canvas->property("plotArea").toRectF().toAlignedRect();
            // The rank line has no colour of its own; the canvas draws it in its foreground colour.
            EXPECT_GT(countPixelsNear(image, plot, canvas->colorFor(*series.front())), 0)
                << metric << ": axes painted but the series line was not";
        }
    }

    // Selection through the real pipeline: two imported scenarios, the graphs on the
    // latest run's scenario; activating the other scenario in the browser moves both
    // the per-run graph and the run-history graph to it without picking a run.
    TEST(ScenarioSelectionUiTest, ActivatingAScenarioMovesBothGraphsToIt) {
        integration::TestEnv env;
        ASSERT_TRUE(env.valid());
        ASSERT_TRUE(env.makePerformancesDir());
        ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
        ASSERT_FALSE(env.copyFixtureIntoPerformances("VT FlyTS Novice S5.perf").isEmpty());
        application::App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.graphConfigurationPersistence,
                             env.statsParser, env.benchmarkStore, env.daySource);
        ASSERT_EQ(app.start(), 0) << "Main.qml failed to load";

        auto *browser = app.scenarioBrowserVm();
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return app.profileService()->isProfileLoaded() && app.profileService()->getScenarioList().size() == 2;
        }, 10000));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !browser->activeScenarioHash().isEmpty(); }, 3000));

        const auto current = app.sessionController()->getCurrentRun().run_id.scenario_id;
        EXPECT_EQ(browser->activeScenarioHash().toStdString(), current.hash)
            << "the browser starts on the scenario the graphs show";

        domain::ScenarioId other;
        for (const auto &scenario: app.profileService()->getScenarioList())
            if (!(scenario == current)) other = scenario;
        ASSERT_FALSE(other.hash.empty());
        const auto otherName = QString::fromStdString(other.name);

        browser->activateScenario(QString::fromStdString(other.hash), otherName);

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return app.sessionController()->getCurrentRun().run_id.scenario_id == other;
        }, 3000)) << "selecting a scenario must make one of its runs current";
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return app.completionHistoryVm()->scenarioTitle() == otherName;
        }, 3000)) << "history graph shows " << app.completionHistoryVm()->scenarioTitle().toStdString();
        EXPECT_TRUE(app.graphVm()->scenarioTitle().startsWith(otherName))
            << "per-run graph shows " << app.graphVm()->scenarioTitle().toStdString();
        EXPECT_EQ(browser->activeScenarioHash().toStdString(), other.hash);
    }

    // before a folder is configured exactly one call to choose it is shown --
    // the Scenarios workspace's full welcome when nothing has been imported, or the
    // banner when runs are already there (e.g. from the default Steam path) -- and it
    // opens the folder dialog; choosing a folder dismisses both.
    TEST(FirstRunUiTest, FolderPromptOpensFolderDialogAndHidesAfterDirectorySelection) {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.remove("file/kovaaks");
        raw.remove("file/kovaaksDirs");
        raw.sync();

        QTemporaryDir tempDir;
        ASSERT_TRUE(tempDir.isValid());
        const auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
        settings->setProfilePath(QDir(tempDir.path()).filePath("profile.pb").toStdString());
        const auto graphConfigurationPersistence = std::make_shared<qt_data::GraphConfigurationPersistence>(settings);
        QTemporaryDir benchmarksDir;
        ASSERT_TRUE(benchmarksDir.isValid());
        application::App app(settings, std::make_shared<data::ProtoDecoder>(), graphConfigurationPersistence,
                             std::make_shared<data::StatsCsvParser>(),
                             std::make_shared<qt_data::BenchmarkStore>(benchmarksDir.path().toStdString()),
                             std::make_shared<tests_support::FakeDaySource>());
        ASSERT_EQ(app.start(), 0) << "Main.qml failed to load";

        auto *root = app.engine()->rootObjects().first();
        auto *banner = root->findChild<QObject *>("firstRunBanner");
        auto *welcome = root->findChild<QObject *>("firstRunWelcome");
        auto *dialog = root->findChild<QObject *>("kovaaksFolderDialog");
        ASSERT_NE(banner, nullptr);
        ASSERT_NE(welcome, nullptr);
        ASSERT_NE(dialog, nullptr);

        const bool nothingImported = !app.scenarioBrowserVm()->hasScenarios();
        EXPECT_EQ(welcome->property("visible").toBool(), nothingImported);
        EXPECT_EQ(banner->property("visible").toBool(), !nothingImported);

        QObject *prompt = nothingImported ? welcome : banner;
        ASSERT_TRUE(QMetaObject::invokeMethod(prompt, "chooseFolderRequested"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000));
        dialog->setProperty("visible", false);

        app.settingsVm()->setKovaaksDir(QUrl::fromLocalFile("D:/Kovaaks"));

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return !banner->property("visible").toBool() && !welcome->property("visible").toBool();
        }, 3000));
    }
}
