//
// Created by Lecka on 27/07/2026.
//


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <app/app.h>
#include <presentation/graph_canvas.h>
#include <presentation/graph_vm.h>

void declare_metatypes() {
    // ksv_ui is a static library built with qt_add_qml_module(NO_PLUGIN); its
    // generated QML_ELEMENT auto-registration object isn't referenced by any
    // other symbol, so the linker drops it and the type never registers.
    // Referencing GraphViewModel here keeps that object linked in and
    // registers it explicitly as a fallback.
    qmlRegisterUncreatableType<ksv::presentation::GraphViewModel>("KovaaksStatsViewer", 1, 0, "GraphViewModel", "Enums only");
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

    return qapp.exec(); // Run the application event loop
}
