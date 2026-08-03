//
// Created by Lecka on 27/07/2026.
//

#include "app.h"

#include <filesystem>

#include <qcoreapplication.h>
#include <QGuiApplication>
#include <QQmlContext>
#include <QStandardPaths>

#include "session_controller.h"
#include "settings_service.h"
#include "formats/protobuf/proto_decoder.h"
#include "formats/protobuf/profile_serializer.h"
#include "qt_data/file_service.h"
#include "../data/profile_service.h"
#include "usecases/graph_use_case.h"


namespace ksv::application {
    App::App(QObject *parent) : QObject(parent) {
        m_protoDecoder = std::make_shared<data::ProtoDecoder>();

        m_settingsService = std::make_shared<qt_data::SettingsService>();
        m_fileService = std::make_shared<qt_data::FileService>(m_settingsService, m_protoDecoder);

        const auto cache_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        std::filesystem::create_directories(cache_dir);
        const std::filesystem::path cache_path = std::filesystem::path(cache_dir) / "profile_cache.pb";

        m_profileService = std::make_shared<data::ProfileService>(
            m_fileService, std::make_shared<data::ProfileSerializer>(), cache_path);
        m_profileService->loadProfile();

        m_sessionController = std::make_shared<SessionController>(m_settingsService, m_profileService);
        m_graphUseCase = std::make_shared<GraphUseCase>(m_sessionController);
        m_graphVm = new presentation::GraphViewModel(m_graphUseCase, this);
        m_sessionVm = new presentation::SessionViewModel(m_sessionController, this);
    }

    int App::start() {
        m_engine.setInitialProperties({
            {"graphVm", QVariant::fromValue(m_graphVm)}, {"sessionVm", QVariant::fromValue(m_sessionVm)}
        });
        m_engine.loadFromModule("KovaaksStatsViewer", "Main");
        if (m_engine.rootObjects().isEmpty()) return -1;
        return 0;
    }
} // Application
