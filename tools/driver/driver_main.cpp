//
// Dev-only UI automation host: builds the real App wiring against a disposable
// session directory, loads Main.qml (from src/ui/qml on disk unless
// --compiled-qml), and serves the UiDriver protocol on a localhost TCP port so
// scripts/ui_driver.py can screenshot and drive the UI. Writes
// <session>/session.json once ready. Not shipped -- gated with ksv_gallery.
//

#include <memory>

#include <QColor>
#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QWindow>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTest>
#include <QUrl>
#include <cstdio>

#include <app/app.h>

#include "driver_dataset.h"
#include "driver_server.h"
#include "logging_setup.h"
#include "message_log.h"
#include "qml_registration.h"
#include "qml_source_interceptor.h"
#include "settings_service.h"
#include "series_config_store.h"
#include "ui_driver.h"
#include "formats/csv/stats_csv_parser.h"
#include "formats/protobuf/proto_decoder.h"
#include "qt_data/benchmark_store.h"

using namespace ksv;

namespace {
    int fail(const int code, const QString &message) {
        std::fprintf(stderr, "ksv_driver: %s\n", qPrintable(message));
        return code;
    }

    // The offscreen QPA has no font directory of its own and no colour-scheme
    // support, so headless runs would render tofu boxes on a light palette.
    // Point it at the Windows fonts before QGuiApplication starts...
    void prepareHeadlessFonts() {
        if (!qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) return;
        const QString windows = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
        qputenv("QT_QPA_FONTDIR", QDir(windows).filePath(QStringLiteral("Fonts")).toUtf8());
    }

    bool isHeadlessPlatform() { return QGuiApplication::platformName() != u"windows"; }

    // ...and the offscreen theme ignores setColorScheme(Dark), while Qt Quick
    // Controls take their palette from the platform theme, not the app palette.
    // This is what the windows QPA produces for the dark scheme (captured from
    // Qt 6.12); headless runs write it into every window's palette after load so
    // screenshots match the real app. Columns: QML name, role, enabled, disabled.
    struct PaletteEntry {
        const char *name;
        QPalette::ColorRole role;
        const char *normal;
        const char *disabled;
    };
    constexpr PaletteEntry kWindowsDarkPalette[]{
        {"window", QPalette::Window, "#1e1e1e", "#1e1e1e"},
        {"windowText", QPalette::WindowText, "#ffffff", "#9d9d9d"},
        {"base", QPalette::Base, "#2d2d2d", "#1e1e1e"},
        {"alternateBase", QPalette::AlternateBase, "#00457b", "#343434"},
        {"text", QPalette::Text, "#ffffff", "#9d9d9d"},
        {"button", QPalette::Button, "#3c3c3c", "#3c3c3c"},
        {"buttonText", QPalette::ButtonText, "#ffffff", "#9d9d9d"},
        {"brightText", QPalette::BrightText, "#65bcff", "#65bcff"},
        {"light", QPalette::Light, "#787878", "#787878"},
        {"midlight", QPalette::Midlight, "#5a5a5a", "#5a5a5a"},
        {"mid", QPalette::Mid, "#282828", "#282828"},
        {"dark", QPalette::Dark, "#1e1e1e", "#1e1e1e"},
        {"shadow", QPalette::Shadow, "#000000", "#000000"},
        {"highlight", QPalette::Highlight, "#00838f", "#00838f"},
        {"highlightedText", QPalette::HighlightedText, "#ffffff", "#ffffff"},
        {"link", QPalette::Link, "#65bcff", "#0000ff"},
        {"linkVisited", QPalette::LinkVisited, "#32a6ff", "#ff00ff"},
        {"toolTipBase", QPalette::ToolTipBase, "#3c3c3c", "#ffffdc"},
        {"toolTipText", QPalette::ToolTipText, "#d4d4d4", "#000000"},
        {"placeholderText", QPalette::PlaceholderText, "#80ffffff", "#80ffffff"},
        {"accent", QPalette::Accent, "#00bcd4", "#00bcd4"},
    };

    // Windows' default UI font plus the app palette (for QPainter-drawn items).
    void applyHeadlessAppLook() {
        if (!isHeadlessPlatform()) return;
        QGuiApplication::setFont(QFont(QStringLiteral("Segoe UI"), 9));
        QPalette palette;
        for (const auto &entry: kWindowsDarkPalette) {
            palette.setColor(QPalette::Active, entry.role, QColor(entry.normal));
            palette.setColor(QPalette::Inactive, entry.role, QColor(entry.normal));
            palette.setColor(QPalette::Disabled, entry.role, QColor(entry.disabled));
        }
        QGuiApplication::setPalette(palette);
    }

    // Writes the palette into each Qt Quick window's `palette` color groups
    // (QQuickPalette / QQuickColorGroup, reached through their meta-properties).
    // Main.qml sets its own accent/highlight on the main window; those are kept.
    void applyHeadlessWindowPalettes(const QObject *mainWindow) {
        if (!isHeadlessPlatform()) return;
        for (QWindow *window: QGuiApplication::topLevelWindows()) {
            auto *palette = window->property("palette").value<QObject *>();
            if (!palette) continue;
            for (const char *group: {"active", "inactive", "disabled"}) {
                auto *colorGroup = palette->property(group).value<QObject *>();
                if (!colorGroup) continue;
                const bool disabled = qstrcmp(group, "disabled") == 0;
                for (const auto &entry: kWindowsDarkPalette) {
                    const bool setByMain = qstrcmp(entry.name, "accent") == 0 || qstrcmp(entry.name, "highlight") == 0;
                    if (setByMain && window == mainWindow) continue;
                    colorGroup->setProperty(entry.name, QColor(disabled ? entry.disabled : entry.normal));
                }
            }
        }
    }

    bool writeSessionFile(const QString &path, const QJsonObject &content) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument(content).toJson(QJsonDocument::Indented));
        return file.commit();
    }
}

int main(int argc, char *argv[]) {
    install_debug_message_pattern();
    driver::MessageLog::install();
    declare_metatypes();
    prepareHeadlessFonts();
    QQuickStyle::setStyle("Fusion");
    QGuiApplication qapp(argc, argv);
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    applyHeadlessAppLook();
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer");

    QCommandLineParser parser;
    parser.setApplicationDescription("KSV UI automation host (see tools/driver/README.md).");
    parser.addHelpOption();
    const QCommandLineOption sessionOption("session-dir", "Session directory (data, settings, session.json).", "dir");
    const QCommandLineOption datasetOption(
        "dataset", "Seed data: " + driver::datasetNames().join(", ") + ". Default: rich.", "name", "rich");
    const QCommandLineOption sizeOption("size", "Main window size, WxH. Default: 1280x800.", "WxH", "1280x800");
    const QCommandLineOption compiledOption("compiled-qml", "Load the compiled QML module instead of src/ui/qml.");
    const QCommandLineOption readyTimeoutOption("ready-timeout-ms", "Max wait for the profile to load.", "ms", "20000");
    parser.addOptions({sessionOption, datasetOption, sizeOption, compiledOption, readyTimeoutOption});
    parser.process(qapp);

    if (!parser.isSet(sessionOption)) return fail(2, "--session-dir is required");
    const QDir sessionDir(QDir(parser.value(sessionOption)).absolutePath());
    const QString dataset = parser.value(datasetOption);
    static const QRegularExpression sizePattern(QStringLiteral("^(\\d+)x(\\d+)$"));
    const auto sizeMatch = sizePattern.match(parser.value(sizeOption));
    if (!sizeMatch.hasMatch()) return fail(2, "--size must look like 1280x800");

    // Every QSettings (SettingsService, QML Settings {}) and AppDataLocation is
    // redirected into the session, so a driver run never touches the real
    // registry or %APPDATA% (same redirect as tests/integration/qt_test_main.cpp).
    const QString settingsDir = sessionDir.filePath("settings");
    if (!QDir().mkpath(settingsDir)) return fail(2, "could not create " + settingsDir);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir);
    QStandardPaths::setTestModeEnabled(true);

    const driver::SessionData data{sessionDir.filePath("data")};
    if (const QString error = driver::seedSessionData(data, dataset, QStringLiteral(KSV_EXAMPLES_DIR));
        !error.isEmpty()) {
        return fail(2, error);
    }

    auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
    if (driver::datasetSetsKovaaksDir(dataset) && !settings->isKovaaksDirSet()) {
        settings->setKovaaksDirs({data.root.toStdString()});
    }
    settings->setProfilePath(data.profilePath().toStdString());

    application::App app(
        settings, std::make_shared<data::ProtoDecoder>(),
        std::make_shared<qt_data::SeriesConfigStore>(settings),
        std::make_shared<data::StatsCsvParser>(),
        std::make_shared<qt_data::BenchmarkStore>(data.benchmarksDir().toStdString()));
    QQmlApplicationEngine *engine = app.engine();

    const bool compiledQml = parser.isSet(compiledOption);
    gallery::QmlSourceInterceptor sourceInterceptor(QStringLiteral(KSV_QML_SOURCE_DIR));
    if (compiledQml) {
        if (app.start() != 0) return fail(3, "Main.qml failed to load (see the log above)");
    } else {
        engine->addUrlInterceptor(&sourceInterceptor);
        engine->setInitialProperties(app.initialProperties());
        engine->load(QUrl(QStringLiteral("qrc:/qt/qml/KovaaksStatsViewer/qml/Main.qml")));
        if (engine->rootObjects().isEmpty()) return fail(3, "Main.qml failed to load (see the log above)");
    }
    QObject *root = engine->rootObjects().first();
    applyHeadlessWindowPalettes(root);
    if (auto *window = qobject_cast<QQuickWindow *>(root)) {
        window->resize(sizeMatch.captured(1).toInt(), sizeMatch.captured(2).toInt());
    }

    const bool profileLoaded = QTest::qWaitFor([&] { return app.profileService()->isProfileLoaded(); },
                                               parser.value(readyTimeoutOption).toInt());
    if (!profileLoaded) qWarning("ksv_driver: profile still loading after the ready timeout; continuing");
    if (auto *window = qobject_cast<QQuickWindow *>(root)) QTest::qWaitForWindowExposed(window, 5000);
    QTest::qWait(200);  // let the first frames and deferred bindings settle

    driver::UiDriver uiDriver(engine, root);
    driver::DriverServer server(uiDriver);
    if (!server.listen()) return fail(4, "could not listen on 127.0.0.1");

    const QJsonObject session{
        {"pid", QCoreApplication::applicationPid()},
        {"port", server.port()},
        {"platform", QGuiApplication::platformName()},
        {"dataset", dataset},
        {"qmlSource", compiledQml ? QStringLiteral("compiled") : QStringLiteral(KSV_QML_SOURCE_DIR)},
        {"sessionDir", sessionDir.absolutePath()},
        {"profileLoaded", profileLoaded},
    };
    if (!writeSessionFile(sessionDir.filePath("session.json"), session)) {
        return fail(4, "could not write session.json");
    }
    std::fprintf(stdout, "ksv_driver ready on 127.0.0.1:%d\n", server.port());
    std::fflush(stdout);

    return QGuiApplication::exec();
}
