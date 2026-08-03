//
// Created by Lecka on 27/07/2026.
//

#ifndef KOVAAKSSTATISTICSVIEWER_APP_H
#define KOVAAKSSTATISTICSVIEWER_APP_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "graph_vm.h"
#include "session_vm.h"
#include "settings_vm.h"
#include "interfaces/i_proto_decoder.h"
#include "../data/interfaces/i_file_service.h"
#include "data/interfaces/i_profile_service.h"
#include "interfaces/i_settings_service.h"
#include "usecases/i_session_controller.h"


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
        presentation::SessionViewModel* m_sessionVm;
        presentation::SettingsViewModel* m_settingsVm;

        std::shared_ptr<ISettingsService> m_settingsService;
        std::shared_ptr<IFileService> m_fileService;
        std::shared_ptr<IProfileService> m_profileService;
        std::shared_ptr<ISessionController> m_sessionController;
        std::shared_ptr<IGraphUseCase> m_graphUseCase;
        std::shared_ptr<IProtoDecoder> m_protoDecoder;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_APP_H