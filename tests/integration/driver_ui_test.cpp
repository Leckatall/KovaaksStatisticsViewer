//
// Drives the dev-tool UiDriver (tools/driver) against the real Main.qml scene:
// selector resolution through StackLayout pages and separate dialog windows,
// the item tree, synthesized clicks/typing, and window screenshots. Built only
// when the dev tools are (KSV_BUILD_GALLERY, non-Release).
//

#include <gtest/gtest.h>

#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "selector.h"
#include "ui_driver.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    TEST(DriverSelectorTest, ParsesWindowTermsAndIndex) {
        const auto selector = driver::parseSelector("win:settingsDialog/type:Button & text:Close > label@1");

        EXPECT_EQ(selector.window, "settingsDialog");
        ASSERT_EQ(selector.segments.size(), 2);
        ASSERT_EQ(selector.segments[0].terms.size(), 2);
        EXPECT_EQ(selector.segments[0].terms[0].kind, driver::SelectorTerm::Kind::Type);
        EXPECT_EQ(selector.segments[0].terms[1].kind, driver::SelectorTerm::Kind::Text);
        EXPECT_EQ(selector.segments[1].terms[0].kind, driver::SelectorTerm::Kind::Name);
        EXPECT_EQ(selector.segments[1].index, 1);
    }

    TEST(DriverSelectorTest, MatchesGlobsAndRejectsEmptyTerms) {
        const auto selector = driver::parseSelector("scenarioItem_*");
        EXPECT_TRUE(selector.segments[0].terms[0].pattern.match("scenarioItem_12").hasMatch());
        EXPECT_FALSE(selector.segments[0].terms[0].pattern.match("xscenarioItem_12").hasMatch());

        EXPECT_THROW(driver::parseSelector("text:"), driver::DriverError);
        EXPECT_THROW(driver::parseSelector("win:/foo"), driver::DriverError);
    }

    struct DriverScene {
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        QObject *root = nullptr;
        std::unique_ptr<driver::UiDriver> uiDriver;

        [[nodiscard]] QString build() {
            if (!env.valid()) return "temp dir invalid";
            app = std::make_unique<application::App>(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore,
                env.statsParser, env.benchmarkStore);
            if (app->start() != 0) return "Main.qml failed to load";
            root = app->engine()->rootObjects().first();
            uiDriver = std::make_unique<driver::UiDriver>(app->engine(), root);
            return {};
        }
    };

    class DriverUiTest : public testing::Test {
    protected:
        static std::unique_ptr<DriverScene> scene;

        static void SetUpTestSuite() {
            scene = std::make_unique<DriverScene>();
            const QString error = scene->build();
            ASSERT_TRUE(error.isEmpty()) << error.toStdString();
        }

        static void TearDownTestSuite() { scene.reset(); }

        void SetUp() override {
            ASSERT_NE(scene, nullptr);
            auto *dialog = scene->root->findChild<QObject *>("settingsDialog");
            if (dialog && dialog->property("visible").toBool()) {
                QMetaObject::invokeMethod(dialog, "close");
                QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 3000);
            }
            scene->root->findChild<QObject *>("workspaceTabBar")->setProperty("currentIndex", 0);
        }

        static QJsonObject run(const QString &command, const QJsonObject &args = {}) {
            return scene->uiDriver->execute(command, args);
        }

        static QJsonObject failure(const QString &command, const QJsonObject &args) {
            try {
                run(command, args);
            } catch (const driver::DriverError &error) {
                return QJsonObject{{"message", error.message()}, {"details", error.details()}};
            }
            ADD_FAILURE() << command.toStdString() << " unexpectedly succeeded";
            return {};
        }
    };

    std::unique_ptr<DriverScene> DriverUiTest::scene;

    TEST_F(DriverUiTest, FindsItemsInsideTheVisibleStackLayoutPage) {
        const auto result = run("find", {{"selector", "workspaceStack > scenarioSearchField"}});

        EXPECT_EQ(result.value("count").toInt(), 1);
        const auto match = result.value("matches").toArray().first().toObject();
        EXPECT_EQ(match.value("name").toString(), "scenarioSearchField");
        EXPECT_EQ(match.value("window").toString(), "main");
    }

    TEST_F(DriverUiTest, HiddenPagesAreExcludedUnlessAsked) {
        const auto hidden = run("find", {{"selector", "benchmarkWorkspace"}});
        EXPECT_EQ(hidden.value("count").toInt(), 0);

        const auto all = run("find", {{"selector", "benchmarkWorkspace"}, {"all", true}});
        EXPECT_EQ(all.value("count").toInt(), 1);
    }

    TEST_F(DriverUiTest, AmbiguousSelectorsListTheirCandidates) {
        const auto error = failure("click", {{"selector", "type:TabButton"}});

        EXPECT_TRUE(error.value("message").toString().contains("ambiguous"));
        EXPECT_EQ(error.value("details").toObject().value("candidates").toArray().size(), 2);
    }

    TEST_F(DriverUiTest, TextSelectorsPreferTheControlOverItsLabel) {
        const auto result = run("get", {{"selector", "text:Benchmarks & type:TabButton"}, {"property", "objectName"}});

        EXPECT_EQ(result.value("value").toString(), "benchmarksTab");
    }

    TEST_F(DriverUiTest, TreeReportsNamedControlsWithGeometry) {
        const auto result = run("tree", {{"selector", "workspaceTabBar"}});
        const QString json = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));

        EXPECT_TRUE(json.contains("\"name\":\"scenariosTab\"")) << json.toStdString();
        EXPECT_TRUE(json.contains("\"rect\":[")) << json.toStdString();
    }

    TEST_F(DriverUiTest, ClickingATabSwitchesTheWorkspace) {
        run("click", {{"selector", "benchmarksTab"}});

        EXPECT_EQ(run("get", {{"selector", "workspaceTabBar"}, {"property", "currentIndex"}}).value("value").toInt(), 1);
        EXPECT_EQ(run("find", {{"selector", "benchmarkWorkspace"}}).value("count").toInt(), 1);
    }

    TEST_F(DriverUiTest, TypingReachesTheFocusedField) {
        run("type", {{"selector", "scenarioSearchField"}, {"text", "Pasu"}, {"clear", true}});

        EXPECT_EQ(run("get", {{"selector", "scenarioSearchField"}, {"property", "text"}}).value("value").toString(), "Pasu");
        run("type", {{"selector", "scenarioSearchField"}, {"text", ""}, {"clear", true}});
    }

    TEST_F(DriverUiTest, DialogWindowsAreListedAndAddressable) {
        run("eval", {{"expression", "settingsDialog.open()"}});

        bool listed = false;
        for (const auto &value: run("windows").value("windows").toArray()) {
            const auto window = value.toObject();
            if (window.value("key").toString() == "settingsDialog") listed = window.value("visible").toBool();
        }
        EXPECT_TRUE(listed);
        // The button and its own label both match text:; single-target commands take the button.
        const auto close = run("get", {{"selector", "win:settingsDialog/text:Close"}, {"property", "text"}});
        EXPECT_EQ(close.value("target").toObject().value("type").toString(), "Button");
        EXPECT_EQ(close.value("target").toObject().value("window").toString(), "settingsDialog");
    }

    TEST_F(DriverUiTest, ScreenshotsCaptureTheWholeWindow) {
        QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const QString path = QDir(dir.path()).filePath("main.png");

        const auto result = run("screenshot", {{"path", path}});

        const QImage image(path);
        ASSERT_FALSE(image.isNull());
        auto *window = qobject_cast<QQuickWindow *>(scene->root);
        ASSERT_NE(window, nullptr);
        EXPECT_EQ(image.size(), window->size() * window->devicePixelRatio());
        EXPECT_EQ(result.value("window").toString(), "main");
    }

    TEST_F(DriverUiTest, ItemScreenshotsAreCroppedToTheItem) {
        QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const QString path = QDir(dir.path()).filePath("tab.png");

        run("screenshot", {{"path", path}, {"selector", "scenariosTab"}, {"pad", 0}});

        const QImage image(path);
        const auto rect = run("find", {{"selector", "scenariosTab"}}).value("matches").toArray().first()
                              .toObject().value("rect").toArray();
        EXPECT_NEAR(image.width(), rect[2].toInt(), 1);
        EXPECT_NEAR(image.height(), rect[3].toInt(), 1);
    }

    TEST_F(DriverUiTest, UnknownCommandsAndPropertiesAreReported) {
        EXPECT_TRUE(failure("bogus", {}).value("message").toString().contains("unknown command"));
        const auto error = failure("get", {{"selector", "workspaceTabBar"}, {"property", "noSuchProperty"}});
        EXPECT_TRUE(error.value("details").toObject().value("properties").toArray().contains("currentIndex"));
    }
}
