//
// Created by Lecka on 27/07/2026.
//

#include "app.h"

#include <qcoreapplication.h>
#include <QGuiApplication>
#include <QQmlContext>

#include "session_controller.h"
#include "formats/protobuf/proto_decoder.h"
#include "qt_data/file_service.h"
#include "../data/profile_service.h"
#include "usecases/graph_use_case.h"


namespace ksv::application {
    App::App(QObject* parent) : QObject(parent) {
        m_protoDecoder = std::make_shared<data::ProtoDecoder>();
        m_graphUseCase = std::make_shared<GraphUseCase>(*m_protoDecoder);
        m_graphVm = new presentation::GraphViewModel(m_graphUseCase, this);
        //TODO: This being circular **cannot** be good
        m_sessionController = std::make_shared<SessionController>(this);
        m_fileService = std::make_shared<qt_data::FileService>(m_sessionController, m_protoDecoder);
        m_profileService = std::make_shared<data::ProfileService>(m_fileService);
        m_sessionController->setProfileService(m_profileService);
        m_sessionVm = new presentation::SessionViewModel(m_sessionController, this);
    }

    int App::start() {
        m_engine.setInitialProperties({{"graphVm", QVariant::fromValue(m_graphVm)}, {"sessionVm", QVariant::fromValue(m_sessionVm)}});
        m_engine.loadFromModule("KovaaksStatsViewer", "Main");
        if (m_engine.rootObjects().isEmpty()) return -1;
        return 0;
    }

} // Application
