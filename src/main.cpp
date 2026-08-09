//
// Created by Lecka on 27/07/2026.
//


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <app/app.h>
#include <ui/components/graph_canvas.h>
#include <presentation/graph_vm.h>
#include <presentation/playtime_graph_vm.h>

void declare_metatypes() {
    // Reference types to prevent linker from dropping auto-registration objects in static QML module
    qmlRegisterUncreatableType<ksv::presentation::GraphViewModel>("KovaaksStatsViewer", 1, 0, "GraphViewModel", "Enums only");
    qmlRegisterUncreatableType<ksv::presentation::PlaytimeGraphViewModel>("KovaaksStatsViewer", 1, 0, "PlaytimeGraphViewModel", "Created in C++");
    qmlRegisterType<ksv::presentation::GraphCanvas>("KovaaksStatsViewer", 1, 0, "GraphCanvas");
}

int main(int argc, char *argv[]) {
    declare_metatypes();
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
