//
// Created by Lecka on 01/08/2026.
//

#include "file_service.h"

#include <qdir.h>

namespace ksv::qt_data {
    FileService::FileService(std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProtoDecoder> decoder, QObject *parent): QObject(parent),
m_settings_service(std::move(settings_service)), m_decoder(std::move(decoder)){
        m_watcher.addPath(get_kovaaks_dir().absolutePath());
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString& path) { notifyFilesChanged(path.toStdString()); });
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
            perfs.push_back(getPerfFromFile(perf_dir.absoluteFilePath(file).toStdString()));
        }
        return perfs;
    }

    domain::ScenarioPerf FileService::getPerfFromFile(const std::string_view filename) const {
        return m_decoder->decode_file(filename);
    }

    domain::ScenarioPerf FileService::getLatestPerf() const {
        //TODO: Consider moving the "find latest" logic into the user profile class
        QDir perf_dir(get_kovaaks_dir());
        if (!perf_dir.cd("FPSAimTrainer/performances")) {
            qDebug() << "Could not cd to performances dir";
            return {};
        }
        const auto latest_file = perf_dir.entryList(QDir::Files, QDir::Time).takeFirst();
        qDebug() << "Latest file: " << latest_file;
        return getPerfFromFile(perf_dir.absoluteFilePath(latest_file).toStdString());
    }
}
