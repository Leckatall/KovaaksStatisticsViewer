//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_FILE_SERVICE_H

#include <qdir.h>
#include <QObject>
#include <qurl.h>

#include "user_profile.h"
#include "app/usecases/i_session_controller.h"
#include "data/interfaces/i_proto_decoder.h"
#include "interfaces/i_file_service.h"

namespace ksv::qt_data {
    class FileService : public QObject, public application::IFileService {
        Q_OBJECT

    public:
        explicit FileService(std::shared_ptr<application::ISessionController> session_controller, std::shared_ptr<application::IProtoDecoder> decoder, QObject *parent = nullptr);

        // [[nodiscard]] std::vector<std::string> get_files_in_dir(std::string_view dir) const;

        [[nodiscard]] QDir get_kovaaks_dir() const {
            return {m_session_controller->getKovaaksDir().data()};
        }

        [[nodiscard]] std::vector<domain::ScenarioPerf> getAllPerfsFromFiles() const override;

    private:
        std::shared_ptr<application::ISessionController> m_session_controller;
        std::shared_ptr<application::IProtoDecoder> m_decoder;
    };
}

#endif //KOVAAKSSTATSVIEWER_FILE_SERVICE_H
