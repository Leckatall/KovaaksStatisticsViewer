//
// Created by Lecka on 27/07/2026.
//


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <app/app.h>

void declare_metatypes() {
}

int main(int argc, char *argv[]) {
    declare_metatypes();
    QGuiApplication qapp(argc, argv);
    ksv::application::App app;
    if (app.start() != 0) return -1;

    return qapp.exec(); // Run the application event loop
}
