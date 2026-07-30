//
// Created by Lecka on 27/07/2026.
//


#include <QApplication>

#include "app/app.h"


void declare_metatypes() {
}

int main(int argc, char *argv[]) {
    declare_metatypes();
    QApplication app(argc, argv); // Initialize Qt application

    // MainWindow window;             // Create MainWindow object
    // window.show(); // Display window
    // application::Application mainApp;
    // mainApp.start();
    ksv::application::App myApp;
    myApp.start();

    return app.exec(); // Run the application event loop
}
