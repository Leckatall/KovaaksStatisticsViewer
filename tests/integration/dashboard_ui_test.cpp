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
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#include "app/app.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/graphs/completion_history_vm.h"
#include "presentation/graphs/graph_vm.h"
#include "settings_service.h"

#include "integration_env.h"
#include "kovaaks_dir.h"
#include "profile_seeding.h"

using namespace ksv;

namespace {
    struct PlottedLine {
        int column;
        // What the Y axis title reads while this line is the labelled one.
        QString title;
        application::AxisId axis;
    };

    // The committed configuration's enabled run lines that have data and sit on a named axis, in list order.
    // GraphCanvas' y-axis column is always a series' SeriesId, not a position.
    [[nodiscard]] std::vector<PlottedLine> linesOnNamedAxes(application::App &app) {
        const auto snapshot = app.graphConfigurationUseCase()->current();
        const auto &document = snapshot.configuration->runSeries;
        std::vector<PlottedLine> lines;
        for (const auto &series: document.series) {
            if (!series.presentation.enabled || !series.yAxisId) continue;
            const int column = static_cast<int>(series.id.value);
            if (app.graphVm()->series({column}).isEmpty()) continue;
            const auto axis = std::ranges::find(document.axes, *series.yAxisId, &application::AxisConfig::id);
            if (axis != document.axes.end() && !axis->name.empty())
                lines.push_back({column, QString::fromStdString(axis->name), axis->id});
        }
        return lines;
    }

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

        void TearDown() override { integration::clearSavedGraphConfiguration(); }
    };

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

    TEST_F(DashboardUiTest, ConfigureActionsOpenTheRunGraphAtLinesAndStyle) {
        auto *menuBar = root()->property("menuBar").value<QObject *>();
        ASSERT_NE(menuBar, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(menuBar, "configureGraphLinesRequested"));

        auto *dialog = root()->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentPage").toString() == "run"
                && dialog->property("currentSection").toString() == "seriesStyle"
                && app()->settingsVm()->selectedGraph() == "run";
        }, 3000));

        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "openProfile"));
        ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "close"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000));

        auto *button = root()->findChild<QObject *>("configureGraphLinesButton");
        ASSERT_NE(button, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(button, "clicked"));
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool() && dialog->property("currentPage").toString() == "run"
                && dialog->property("currentSection").toString() == "seriesStyle"
                && app()->settingsVm()->selectedGraph() == "run";
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

    // A gear's Hide graph reaches the saved visibility through the real panels and workspace, and its
    // notice undoes it.
    TEST_F(DashboardUiTest, QuickHideFromAGearCanBeUndone) {
        auto *playtime = root()->findChild<QObject *>("playtimeGraphPanel");
        auto *notice = root()->findChild<QObject *>("graphHiddenNotice_dailyPlaytime");
        QObject *visualSettings = root()->findChild<QObject *>("visualSettings");
        ASSERT_TRUE(playtime && notice && visualSettings);
        auto *gear = playtime->findChild<QObject *>("graphGear_dailyPlaytime");
        ASSERT_TRUE(gear);

        ASSERT_TRUE(QMetaObject::invokeMethod(gear, "hideGraphRequested", Q_ARG(QString, "dailyPlaytime")));
        EXPECT_FALSE(visualSettings->property("playtimeGraphVisible").toBool());
        EXPECT_FALSE(playtime->property("visible").toBool());
        EXPECT_TRUE(notice->property("visible").toBool());

        ASSERT_TRUE(QMetaObject::invokeMethod(notice, "undo"));
        EXPECT_TRUE(visualSettings->property("playtimeGraphVisible").toBool());
        EXPECT_TRUE(playtime->property("visible").toBool());
        EXPECT_FALSE(notice->property("visible").toBool());
    }

    TEST_F(DashboardUiTest, HidingEveryScenarioGraphOffersRecovery) {
        auto *workspace = root()->findChild<QObject *>("scenarioWorkspace");
        QObject *visualSettings = root()->findChild<QObject *>("visualSettings");
        ASSERT_TRUE(workspace && visualSettings);
        auto *recovery = workspace->findChild<QObject *>("noGraphsShown");
        ASSERT_TRUE(recovery);
        EXPECT_FALSE(recovery->property("visible").toBool());

        const std::array graphs{"scenarioGraphVisible", "scenarioHistoryGraphVisible", "playtimeGraphVisible"};
        for (const auto *graph: graphs) visualSettings->setProperty(graph, false);
        EXPECT_TRUE(recovery->property("visible").toBool());
        for (const auto *graph: graphs) visualSettings->setProperty(graph, true);
        EXPECT_FALSE(recovery->property("visible").toBool());
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
        const int quickWindow = application::kDefaultAveragingWindowDays + 2;
        const auto quick = settings->applyQuickPatch("dailyPlaytime", {{"window", quickWindow}});
        ASSERT_TRUE(quick["accepted"].toBool());
        EXPECT_EQ(scene.app->playtimeVm()->averagingWindowDays(), quickWindow);

        auto *content = popup->property("contentItem").value<QQuickItem *>();
        auto *windowField = findItem(content, "graphField_dataRange_averagingWindow");
        ASSERT_NE(windowField, nullptr);
        EXPECT_EQ(windowField->property("text").toString(), QString::number(quickWindow));
        auto *more = findItem(content, "graphMoreSettings_dailyPlaytime");
        ASSERT_NE(more, nullptr);
        ASSERT_TRUE(QMetaObject::invokeMethod(more, "clicked"));

        auto *dialog = scene.root->findChild<QObject *>("settingsDialog");
        ASSERT_NE(dialog, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return dialog->property("visible").toBool()
                && dialog->property("currentPage").toString() == "dailyPlaytime";
        }, 3000));
        EXPECT_FALSE(popup->property("opened").toBool());
        EXPECT_EQ(settings->selectedGraph(), "dailyPlaytime");
        EXPECT_TRUE(settings->isVisitOpen());
        EXPECT_FALSE(settings->hasPendingChanges());
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

    TEST_F(DashboardCleanSceneTest, YAxisTitleLabelNamesTheSharedAxis) {
        auto *app = scene.app.get();
        app->graphVm()->fetchData(scene.perfUrl);

        auto *canvas = scene.canvasFor(app->graphVm());
        ASSERT_NE(canvas, nullptr);
        QObject *label = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            label = scene.root->findChild<QObject *>("scenarioYAxisTitleLabel");
            return label != nullptr;
        }, 2000)) << "no label named 'scenarioYAxisTitleLabel' found after loading a perf";
        const auto lines = linesOnNamedAxes(*app);
        ASSERT_FALSE(lines.empty());
        const auto other = std::ranges::find_if(lines, [&](const auto &line) { return line.axis != lines.front().axis; });
        ASSERT_NE(other, lines.end()) << "the committed lines need plotted lines on two named axes";

        canvas->setYAxisColumn(lines.front().column);
        ASSERT_TRUE(QTest::qWaitFor([&] { return label->property("text").toString() == lines.front().title; }, 2000));

        canvas->setYAxisColumn(other->column);

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return label->property("text").toString() == other->title;
        }, 2000)) << "axis title was '" << label->property("text").toString().toStdString() << "'";
    }

    TEST_F(DashboardCleanSceneTest, RenamingTheSharedAxisUpdatesTheYAxisTitle) {
        auto *app = scene.app.get();
        app->graphVm()->fetchData(scene.perfUrl);

        auto *canvas = scene.canvasFor(app->graphVm());
        ASSERT_NE(canvas, nullptr);
        const auto lines = linesOnNamedAxes(*app);
        ASSERT_FALSE(lines.empty());
        const auto &shared = lines.front();
        canvas->setYAxisColumn(shared.column);
        ASSERT_EQ(canvas->property("labelledYAxisTitle").toString(), shared.title);

        const auto snapshot = app->graphConfigurationUseCase()->current();
        ASSERT_NE(snapshot.configuration, nullptr);
        auto candidate = *snapshot.configuration;
        const auto axis = std::ranges::find_if(candidate.runSeries.axes, [&](const auto &a) {
            return a.id == shared.axis;
        });
        ASSERT_NE(axis, candidate.runSeries.axes.end());
        axis->name = "Renamed Axis";
        ASSERT_TRUE(app->graphConfigurationUseCase()->save(candidate).accepted());

        EXPECT_EQ(canvas->property("labelledYAxisTitle").toString(), "Renamed Axis");
    }

    TEST_F(DashboardCleanSceneTest, RenamingAnAxisInSettingsAndSavingRetitlesTheYAxis) {
        auto *app = scene.app.get();
        app->graphVm()->fetchData(scene.perfUrl);
        auto *canvas = scene.canvasFor(app->graphVm());
        ASSERT_NE(canvas, nullptr);
        const auto lines = linesOnNamedAxes(*app);
        ASSERT_FALSE(lines.empty());
        canvas->setYAxisColumn(lines.front().column);
        ASSERT_EQ(canvas->property("labelledYAxisTitle").toString(), lines.front().title);

        auto *settings = app->settingsVm();
        settings->beginVisit();
        const auto renamed = settings->renameAxis(QString::number(lines.front().axis.value), "  Renamed  ");
        ASSERT_TRUE(renamed["succeeded"].toBool());
        ASSERT_TRUE(settings->save()["accepted"].toBool());
        settings->endVisit();

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return canvas->property("labelledYAxisTitle").toString() == "Renamed";
        }, 2000)) << "axis title was '" << canvas->property("labelledYAxisTitle").toString().toStdString() << "'";
    }

    TEST_F(DashboardCleanSceneTest, ARejectedAxisNameDoesNotReachTheGraph) {
        auto *app = scene.app.get();
        const auto &axes = app->graphConfigurationUseCase()->current().configuration->runSeries.axes;
        ASSERT_GE(axes.size(), 2u);
        const auto first = axes[0];
        const auto second = axes[1];

        auto *settings = app->settingsVm();
        settings->beginVisit();
        const auto result = settings->renameAxis(QString::number(first.id.value),
                                                 QString::fromStdString(second.name).toUpper());
        EXPECT_FALSE(result["succeeded"].toBool());
        EXPECT_FALSE(settings->save()["accepted"].toBool());
        settings->endVisit();

        const auto &committed = app->graphConfigurationUseCase()->current().configuration->runSeries.axes;
        const auto kept = std::ranges::find(committed, first.id, &application::AxisConfig::id);
        ASSERT_NE(kept, committed.end());
        EXPECT_EQ(kept->name, first.name);
    }

    TEST_F(DashboardCleanSceneTest, DeletingTheShownAxisInSettingsLabelsItsFirstEnabledLine) {
        auto *app = scene.app.get();
        const auto before = *app->graphConfigurationUseCase()->current().configuration;
        const auto &document = before.runSeries;
        const auto enabled = [&](const application::SeriesId id) {
            return std::ranges::find(document.series, id, &application::SeriesConfig::id)->presentation.enabled;
        };
        const auto firstVisible = std::ranges::find_if(document.series, [](const auto &s) { return s.presentation.enabled; });
        ASSERT_NE(firstVisible, document.series.end());
        // An axis whose first enabled line is not the graph's first visible line, so a cleared label
        // (which falls back to that line) cannot pass for the hand-over.
        const auto axisId = std::ranges::find_if(document.axes, [&](const auto &axis) {
            const auto lines = application::runAxisLabelLines(document, application::RunAxisLabel{axis.id});
            const auto first = std::ranges::find_if(lines, enabled);
            return first != lines.end() && *first != firstVisible->id;
        });
        ASSERT_NE(axisId, document.axes.end());
        const auto onAxis = application::runAxisLabelLines(document, application::RunAxisLabel{axisId->id});
        const auto firstEnabled = std::ranges::find_if(onAxis, enabled);
        ASSERT_NE(firstEnabled, onAxis.end());
        const auto expectedLabel = QString::number(firstEnabled->value);
        const auto axisKey = QString::number(axisId->id.value);

        auto *settings = app->settingsVm();
        settings->beginVisit();
        settings->setRunAxisLabel("axis:" + axisKey);
        ASSERT_TRUE(settings->save()["accepted"].toBool());
        ASSERT_TRUE(settings->deleteAxis(axisKey)["succeeded"].toBool());
        ASSERT_TRUE(settings->save()["accepted"].toBool());
        settings->endVisit();

        EXPECT_EQ(app->graphVm()->axisLabelSeriesIds(), QStringList{expectedLabel});
        const auto &after = app->graphConfigurationUseCase()->current().configuration->runSeries;
        for (const auto id: onAxis) {
            const auto line = std::ranges::find(after.series, id, &application::SeriesConfig::id);
            ASSERT_NE(line, after.series.end());
            EXPECT_FALSE(line->yAxisId.has_value());
        }
    }

    TEST_F(DashboardCleanSceneTest, CancellingSettingsRestoresTheSavedYAxisChoice) {
        auto *app = scene.app.get();
        const auto committedLabel = app->graphConfigurationUseCase()->current().configuration->runAxisLabel;

        auto *settings = app->settingsVm();
        settings->beginVisit();
        const auto overview = settings->axisOverview();
        const auto shown = overview["shownKey"].toString();
        QString other;
        for (const auto &row: overview["sharedAxes"].toList())
            if (row.toMap()["key"].toString() != shown) other = row.toMap()["key"].toString();
        ASSERT_FALSE(other.isEmpty());
        const auto savedKey = settings->runAxisLabelKey();
        settings->setRunAxisLabel(other);
        ASSERT_EQ(settings->runAxisLabelKey(), other);

        settings->revertChanges();

        EXPECT_EQ(settings->runAxisLabelKey(), savedKey);
        EXPECT_EQ(settings->axisOverview()["shownKey"].toString(), shown);
        EXPECT_FALSE(settings->hasPendingChanges());
        ASSERT_TRUE(settings->save()["accepted"].toBool());
        settings->endVisit();

        EXPECT_EQ(app->graphConfigurationUseCase()->current().configuration->runAxisLabel, committedLabel);
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
        ASSERT_EQ(ksv::tests_support::seedProfile(*app->profileService(), std::move(profile)), application::ProfileImportOutcome::Succeeded);

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
        ASSERT_EQ(ksv::tests_support::seedProfile(*app->profileService(), std::move(profile)), application::ProfileImportOutcome::Succeeded);

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
        ASSERT_EQ(ksv::tests_support::seedProfile(*app->profileService(), std::move(profile)), application::ProfileImportOutcome::Succeeded);

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

    // while the Kovaaks folder is invalid exactly one call to choose it is shown --
    // the Scenarios workspace's full welcome when nothing has been imported, or the
    // banner when runs are already there (e.g. from an existing profile) -- and it
    // opens the folder dialog; choosing a valid folder dismisses both.
    TEST(FirstRunUiTest, FolderPromptOpensFolderDialogAndHidesAfterDirectorySelection) {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.remove("file/kovaaks");
        raw.remove("file/kovaaksDirs");
        raw.sync();

        QTemporaryDir tempDir;
        ASSERT_TRUE(tempDir.isValid());
        const auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
        settings->setFallbackKovaaksDir(QDir(tempDir.path()).filePath("no-kovaaks-install").toStdString());
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
        auto *banner = root->findChild<QObject *>("invalidKovaaksDirBanner");
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

        tests_support::KovaaksDir kovaaks;
        ASSERT_TRUE(kovaaks.valid());
        ASSERT_TRUE(kovaaks.makeStatsDir());
        app.settingsVm()->setKovaaksDir(QUrl::fromLocalFile(kovaaks.root()));

        EXPECT_TRUE(QTest::qWaitFor([&] {
            return !banner->property("visible").toBool() && !welcome->property("visible").toBool();
        }, 3000));
    }
}
