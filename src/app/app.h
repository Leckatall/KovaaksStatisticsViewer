//
// Created by Lecka on 27/07/2026.
//

#ifndef KOVAAKSSTATISTICSVIEWER_APP_H
#define KOVAAKSSTATISTICSVIEWER_APP_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "graph_vm.h"
#include "completion_history_vm.h"
#include "playtime_graph_vm.h"
#include "scenario_browser_vm.h"
#include "session_vm.h"
#include "settings_vm.h"
#include "interfaces/i_proto_decoder.h"
#include "../data/interfaces/i_file_service.h"
#include "data/interfaces/i_profile_service.h"
#include "interfaces/i_settings_service.h"
#include "data/interfaces/i_graph_line_config.h"
#include "contracts/i_graph_column_preferences.h"
#include "usecases/i_session_controller.h"
#include "contracts/i_playtime_graph_use_case.h"
#include "contracts/i_completion_history_use_case.h"
#include "contracts/i_scenario_browser_use_case.h"


namespace ksv::application {
    class App: public QObject {
        Q_OBJECT
    public:
        explicit App(QObject* parent = nullptr);
        // Injects the leaf services so tests can drive the real wiring with
        // deterministic paths instead of the real registry / AppDataLocation.
        App(std::shared_ptr<ISettingsService> settingsService,
            std::shared_ptr<IProtoDecoder> decoder,
            std::shared_ptr<IGraphLineConfig> graphLineConfig,
            QObject* parent = nullptr);
        int start();
        QQmlApplicationEngine* engine() {return &m_engine;}

        [[nodiscard]] presentation::GraphViewModel* graphVm() const { return m_graphVm; }
        [[nodiscard]] presentation::PlaytimeGraphViewModel* playtimeVm() const { return m_playtimeVm; }
        [[nodiscard]] presentation::CompletionHistoryViewModel* completionHistoryVm() const {
            return m_completionHistoryVm;
        }
        [[nodiscard]] presentation::SessionViewModel* sessionVm() const { return m_sessionVm; }
        [[nodiscard]] presentation::SettingsViewModel* settingsVm() const { return m_settingsVm; }
        [[nodiscard]] presentation::ScenarioBrowserViewModel* scenarioBrowserVm() const { return m_scenarioBrowserVm; }
        [[nodiscard]] std::shared_ptr<ISettingsService> settingsService() const { return m_settingsService; }
        [[nodiscard]] std::shared_ptr<IProfileService> profileService() const { return m_profileService; }
        [[nodiscard]] std::shared_ptr<ISessionController> sessionController() const { return m_sessionController; }

    private:
        // void initConnections();
        // void setStatusBarMessage() const;
        QQmlApplicationEngine m_engine;
        presentation::GraphViewModel* m_graphVm;
        presentation::PlaytimeGraphViewModel* m_playtimeVm;
        presentation::CompletionHistoryViewModel* m_completionHistoryVm;
        presentation::SessionViewModel* m_sessionVm;
        presentation::SettingsViewModel* m_settingsVm;
        presentation::ScenarioBrowserViewModel* m_scenarioBrowserVm;

        std::shared_ptr<ISettingsService> m_settingsService;
        std::shared_ptr<IGraphLineConfig> m_graphLineConfig;
        std::shared_ptr<IGraphColumnPreferences> m_graphColumnPreferences;
        std::shared_ptr<IFileService> m_fileService;
        std::shared_ptr<IProfileService> m_profileService;
        std::shared_ptr<ISessionController> m_sessionController;
        std::shared_ptr<IGraphUseCase> m_graphUseCase;
        std::shared_ptr<IPlaytimeGraphUseCase> m_playtimeUseCase;
        std::shared_ptr<ICompletionHistoryUseCase> m_completionHistoryUseCase;
        std::shared_ptr<IScenarioBrowserUseCase> m_scenarioBrowserUseCase;
        std::shared_ptr<IProtoDecoder> m_protoDecoder;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_APP_H
