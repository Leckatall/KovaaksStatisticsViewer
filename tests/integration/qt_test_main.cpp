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
#include <QStyleHints>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "qml_registration.h"

int main(int argc, char **argv) {
    ksv::declare_metatypes();
    QQuickStyle::setStyle("Fusion");

    QGuiApplication app(argc, argv);
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
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
