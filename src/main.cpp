//
// Created by Lecka on 27/07/2026.
//


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include <app/app.h>

#include "qml_registration.h"

int main(int argc, char *argv[]) {
    ksv::declare_metatypes();
    QQuickStyle::setStyle("Fusion");
    QGuiApplication qapp(argc, argv);
    // Fusion takes its colours from the system palette; pin the scheme so the app
    // stays dark regardless of the OS light/dark setting. Needs the QGuiApplication.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer");
    QCoreApplication::setApplicationVersion(KSV_VERSION);
    QGuiApplication::setOrganizationName("Lecka");
    QGuiApplication::setApplicationName("KovaaksStatsViewer");
    QGuiApplication::setApplicationVersion(KSV_VERSION);
    ksv::application::App app;
    if (app.start() != 0) return -1;

    return qapp.exec();
}
