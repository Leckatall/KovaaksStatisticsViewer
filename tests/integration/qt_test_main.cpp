//
// Integration-suite main. Unlike the other suites' bare-QCoreApplication mains,
// this builds a QGuiApplication (QtQuick items / Main.qml need it), registers
// the C++ QML types exactly as src/main.cpp does, and redirects ALL QSettings
// storage to a temp dir so nothing here ever touches the real registry.
//

#include <QGuiApplication>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "components/graph_canvas.h"
#include "presentation/graph_vm.h"
#include "presentation/playtime_graph_vm.h"

namespace {
    // Mirrors declare_metatypes() in src/main.cpp: referencing the types forces
    // the linker to keep the static QML-module auto-registration objects that a
    // NO_PLUGIN static lib would otherwise drop.
    void registerQmlTypes() {
        qmlRegisterUncreatableType<ksv::presentation::GraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "GraphViewModel", "Enums only");
        qmlRegisterUncreatableType<ksv::presentation::PlaytimeGraphViewModel>(
            "KovaaksStatsViewer", 1, 0, "PlaytimeGraphViewModel", "Created in C++");
        qmlRegisterType<ksv::presentation::GraphCanvas>("KovaaksStatsViewer", 1, 0, "GraphCanvas");
    }
}

int main(int argc, char **argv) {
    registerQmlTypes();
    QQuickStyle::setStyle("Fusion");

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer");

    // Redirect QSettings (including QML QtCore Settings in Main.qml, and any
    // NativeFormat SettingsService) to a throwaway dir for the whole run.
    static QTemporaryDir settings_dir;
    if (settings_dir.isValid()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());
    }
    // Keep AppDataLocation off the real profile too, in case any path resolves there.
    QStandardPaths::setTestModeEnabled(true);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
