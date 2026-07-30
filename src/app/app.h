//
// Created by Lecka on 27/07/2026.
//

#ifndef KOVAAKSSTATISTICSVIEWER_APP_H
#define KOVAAKSSTATISTICSVIEWER_APP_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "graph_vm.h"


namespace ksv::application {
    class App: public QObject {
        Q_OBJECT
    public:
        explicit App(QObject* parent = nullptr);
        int start();

    private:
        // void initConnections();
        // void setStatusBarMessage() const;
        QQmlApplicationEngine m_engine;
        presentation::GraphViewModel* m_graphVm;
    };
} // Application

#endif //KOVAAKSSTATISTICSVIEWER_APP_H