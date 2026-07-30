//
// Created by Lecka on 27/07/2026.
//

#include "app.h"

#include <qcoreapplication.h>
#include <qdir.h>
#include <QGuiApplication>
#include <QQmlContext>


namespace ksv::application {
    App::App(QObject* parent) : QObject(parent), m_graphVm(new presentation::GraphViewModel(this)) {

    }

    int App::start() {
        m_engine.setInitialProperties({{"graphVm", QVariant::fromValue(m_graphVm)}});
        // m_engine.addImportPath(QDir(QGuiApplication::applicationDirPath())
        //              .absoluteFilePath("src/ui"));
        m_engine.loadFromModule("KovaaksStatsViewer", "Main");
        if (m_engine.rootObjects().isEmpty()) return -1;
        return 0;
    }

} // Application
