//
// Created by Lecka on 01/08/2026.
//

#include "file_service.h"

#include <qdir.h>

namespace ksv::qt_data {
    FileService::FileService(std::shared_ptr<application::ISessionController> session_controller,
                             std::shared_ptr<application::IProtoDecoder> decoder,
                             QObject *parent) : QObject(parent),
                                                m_session_controller(std::move(session_controller)),
                                                m_decoder(std::move(decoder)) {
    }

    std::vector<domain::ScenarioPerf> FileService::getAllPerfsFromFiles() const {
        QDir perf_dir(get_kovaaks_dir());
        if (!perf_dir.cd("FPSAimTrainer/performances")) {
            qDebug() << "Could not cd to performances dir";
            return {};
        }
        auto files = perf_dir.entryList(QDir::Files);
        std::vector<domain::ScenarioPerf> perfs;
        for (const auto &file: files) {
            perfs.push_back(m_decoder->decode_file(perf_dir.absoluteFilePath(file).toStdString()));
        }
        return perfs;
    }
}
