//
// Created by Lecka on 27/07/2026.
//

#ifndef KOVAAKSSTATISTICSVIEWER_APP_H
#define KOVAAKSSTATISTICSVIEWER_APP_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "graph_vm.h"
#include "interfaces/i_proto_decoder.h"


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
        std::shared_ptr<IGraphUseCase> m_graphUseCase;
        std::shared_ptr<IProtoDecoder> m_protoDecoder;
    };
} // Application

#endif //KOVAAKSSTATISTICSVIEWER_APP_H