//
// QML smoke test: boots the REAL App (real view models, real QML module load of
// Main.qml) under the offscreen/software backend and asserts the scene graph
// actually instantiates. This is what catches declare_metatypes() registration
// regressions and real-VM binding breakage that the JS-fake ui_qml_tests can't,
// because those never touch the C++ types or the compiled QML module.
//

#include <gtest/gtest.h>

#include <QFile>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <memory>
#include <vector>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    std::vector<QString> g_messages;

    void captureHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
        if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
            g_messages.push_back(msg);
        }
    }

    // Substrings that mark a genuine registration/binding failure (as opposed to
    // cosmetic style chatter from Fusion under offscreen).
    bool looksLikeLoadFailure(const QString &msg) {
        static const char *signatures[] = {
            "is not a type", "Cannot assign", "Unable to assign", "ReferenceError",
            "required property", "Cannot read property", "isn't a valid",
            "module \"KovaaksStatsViewer\"", "GraphCanvas", "GraphViewModel",
        };
        for (const auto *sig: signatures) {
            if (msg.contains(QLatin1String(sig))) return true;
        }
        return false;
    }

    TEST(QmlSmokeTest, MainLoadsWithRealViewModels) {
        integration::TestEnv env;
        ASSERT_TRUE(env.valid());

        g_messages.clear();
        QtMessageHandler previous = qInstallMessageHandler(captureHandler);

        int startResult = -1;
        {
            application::App app(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
            startResult = app.start();
        }

        qInstallMessageHandler(previous);

        EXPECT_EQ(startResult, 0) << "App::start() reported no root QML objects — Main.qml failed to load";

        for (const auto &msg: g_messages) {
            EXPECT_FALSE(looksLikeLoadFailure(msg)) << "QML load/binding failure: " << msg.toStdString();
        }
    }

    bool mainQmlRequires(const QString &propertyName) {
        QFile file(QStringLiteral(KSV_UI_QML_DIR "/Main.qml"));
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QString source = QString::fromUtf8(file.readAll());
        return QRegularExpression(
                   QStringLiteral("required\\s+property\\s+[\\w.<>]+\\s+%1\\b").arg(propertyName))
            .match(source)
            .hasMatch();
    }

    TEST(QmlSmokeTest, MainRequiresBothBenchmarkViewModels) {
        integration::TestEnv env;
        ASSERT_TRUE(env.valid());

        EXPECT_TRUE(mainQmlRequires(QStringLiteral("benchmarkTrackingVm")))
            << "Main.qml must declare benchmarkTrackingVm as a required property";
        EXPECT_TRUE(mainQmlRequires(QStringLiteral("benchmarkManagerVm")))
            << "Main.qml must declare benchmarkManagerVm as a required property";

        application::App app(
            env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
        ASSERT_EQ(app.start(), 0) << "Main.qml failed to load";
        ASSERT_FALSE(app.engine()->rootObjects().isEmpty());

        auto *root = app.engine()->rootObjects().first();
        EXPECT_EQ(root->property("benchmarkTrackingVm").value<QObject *>(),
                  static_cast<QObject *>(app.benchmarkTrackingVm()));
        EXPECT_EQ(root->property("benchmarkManagerVm").value<QObject *>(),
                  static_cast<QObject *>(app.benchmarkManagerVm()));
    }
}
