//
// Created by Lecka on 27/07/2026.
//

#include "app.h"
#include <QStatusBar>

#include "components/main/menu_bar.h"

namespace ksv::application {
    App::App() : QObject(nullptr),
                 m_window(new QMainWindow()),
                 m_container(new QFrame()) {
        m_window->setWindowTitle("ChessRepo");
        m_window->setGeometry(0, 0, 1200, 800);
        m_window->setCentralWidget(m_container);
        m_window->setMenuBar(new ui::mainwindow::WindowMenuBar());
        // setStatusBarMessage();
        initLayout();
        // initConnections();
    }

    void App::start() const {
        m_window->show();
    }

    void App::initLayout() const {
        const auto layout = new QGridLayout(m_container);
        m_container->setLayout(layout);
    }
} // Application
