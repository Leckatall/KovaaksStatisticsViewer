//
// Created by Lecka on 27/07/2026.
//

#include "app.h"

#include <filesystem>

#include <qcoreapplication.h>
#include <QGuiApplication>
#include <QQmlContext>

#include "session_controller.h"
#include "settings_service.h"
#include "formats/protobuf/proto_decoder.h"
#include "formats/protobuf/profile_serializer.h"
#include "qt_data/file_service.h"
#include "../data/profile_service.h"
#include "usecases/graph_use_case.h"
#include "usecases/completion_history_use_case.h"
#include "usecases/playtime_graph_use_case.h"


namespace ksv::application {
    App::App(QObject *parent)
        : App(std::make_shared<qt_data::SettingsService>(), std::make_shared<data::ProtoDecoder>(), parent) {}

    App::App(std::shared_ptr<ISettingsService> settingsService,
             std::shared_ptr<IProtoDecoder> decoder, QObject *parent) : QObject(parent) {
        m_protoDecoder = std::move(decoder);

        m_settingsService = std::move(settingsService);
        m_fileService = std::make_shared<qt_data::FileService>(m_settingsService, m_protoDecoder);

        m_profileService = std::make_shared<data::ProfileService>(
            m_fileService, std::make_shared<data::ProfileSerializer>(), m_settingsService);

        // SessionController installs the build requester, so it has to exist before the
        // first loadProfile() — otherwise a missing stored profile builds synchronously and blocks
        // startup for as long as a full directory scan takes.
        m_sessionController = std::make_shared<SessionController>(m_settingsService, m_profileService, m_fileService);
        m_profileService->loadProfile();

        m_graphUseCase = std::make_shared<GraphUseCase>(m_sessionController);
        m_graphVm = new presentation::GraphViewModel(m_graphUseCase, this);
        // Re-pull the series when currentPerf changes for any reason (file load, run selection, latest-on-startup)
        m_graphUseCase->onCurrentPerfChanged([this] { m_graphVm->fetchData(); });
        // SessionController already loaded the latest perf in its own constructor, before the
        // connection above existed, so that first currentPerfChanged was never observed here.
        m_graphVm->fetchData();

        m_playtimeUseCase = std::make_shared<PlaytimeGraphUseCase>(m_profileService);
        m_playtimeVm = new presentation::PlaytimeGraphViewModel(m_playtimeUseCase, this);
        // Re-pull rolling average when profile changes (new run, store reload, or dir change)
        m_profileService->onProfileChanged([this] { m_playtimeVm->refresh(); });

        m_completionHistoryUseCase = std::make_shared<CompletionHistoryUseCase>(
            m_sessionController, m_profileService);
        m_completionHistoryVm = new presentation::CompletionHistoryViewModel(m_completionHistoryUseCase, this);
        m_completionHistoryUseCase->onCurrentScenarioChanged([this] { m_completionHistoryVm->refresh(); });

        m_sessionVm = new presentation::SessionViewModel(m_sessionController, this);
        m_settingsVm = new presentation::SettingsViewModel(m_settingsService, m_profileService, this);
        m_scenarioBrowserVm = new presentation::ScenarioBrowserViewModel(m_sessionController, this);
    }

    int App::start() {
        m_engine.setInitialProperties({
            {"graphVm", QVariant::fromValue(m_graphVm)},
            {"playtimeVm", QVariant::fromValue(m_playtimeVm)},
            {"historyVm", QVariant::fromValue(m_completionHistoryVm)},
            {"sessionVm", QVariant::fromValue(m_sessionVm)},
            {"settingsVm", QVariant::fromValue(m_settingsVm)},
            {"scenarioBrowserVm", QVariant::fromValue(m_scenarioBrowserVm)}
        });
        m_engine.loadFromModule("KovaaksStatsViewer", "Main");
        if (m_engine.rootObjects().isEmpty()) return -1;
        return 0;
    }
} // Application
