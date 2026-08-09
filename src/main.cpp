//
// Created by Lecka on 27/07/2026.
//


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <app/app.h>

#include "qml_registration.h"

int main(int argc, char *argv[]) {
    ksv::declare_metatypes();
    QQuickStyle::setStyle("Fusion");
    QGuiApplication qapp(argc, argv);
    QCoreApplication::setOrganizationName("Lecka");
    QCoreApplication::setApplicationName("KovaaksStatsViewer");
    QGuiApplication::setOrganizationName("Lecka");
    QGuiApplication::setApplicationName("KovaaksStatsViewer");
    ksv::application::App app;
    if (app.start() != 0) return -1;

    return qapp.exec();
}
