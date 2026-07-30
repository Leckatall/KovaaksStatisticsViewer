//
// Created by Lecka on 27/07/2026.
//

#include "app.h"

#include <qcoreapplication.h>
#include <qdir.h>
#include <QGuiApplication>
#include <QQmlContext>

#include "formats/protobuf/proto_decoder.h"
#include "usecases/graph_use_case.h"


namespace ksv::application {
    App::App(QObject* parent) : QObject(parent) {
        m_protoDecoder = std::make_shared<data::ProtoDecoder>();
        m_graphUseCase = std::make_shared<GraphUseCase>(*m_protoDecoder);
        m_graphVm = new presentation::GraphViewModel(m_graphUseCase, this);
    }

    int App::start() {
        m_engine.setInitialProperties({{"graphVm", QVariant::fromValue(m_graphVm)}});
        m_engine.loadFromModule("KovaaksStatsViewer", "Main");
        if (m_engine.rootObjects().isEmpty()) return -1;
        return 0;
    }

} // Application
