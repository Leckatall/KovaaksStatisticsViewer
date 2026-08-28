//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_FILE_SERVICE_H

#include <optional>
#include <qdir.h>
#include <qfilesystemwatcher.h>
#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <qurl.h>

#include "user_profile.h"
#include "app/usecases/i_session_controller.h"
#include "data/interfaces/i_proto_decoder.h"
#include "data/interfaces/i_file_service.h"
#include "interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class FileService : public QObject, public application::IFileService {
        Q_OBJECT

    public:
        explicit FileService(std::shared_ptr<application::ISettingsService> settings_service, std::shared_ptr<application::IProtoDecoder> decoder, QObject *parent = nullptr);

        [[nodiscard]] std::vector<application::PerfFile> listPerfFiles() const override;
        [[nodiscard]] domain::ScenarioPerf getPerfFromFile(std::string_view filename) const override;
        [[nodiscard]] std::vector<std::string> sourceRoots() const override;

        void onFilesChanged(std::function<void(const application::PerfFile &)> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        struct PerfDir {
            std::string root;
            std::string subdir;
            QDir directory;
        };

        [[nodiscard]] std::vector<PerfDir> perfDirs() const;

        void notifyFilesChanged(const application::PerfFile &file) const {
            for (auto& cb : m_callbacks) cb(file);
        }

        void handleDirectoryChanged(const QString &directory);

        void watchPerfDirs();
        void repointWatcher();

        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProtoDecoder> m_decoder;

        QFileSystemWatcher m_watcher;
        std::vector<std::function<void(const application::PerfFile &)>> m_callbacks;
        QHash<QString, QSet<QString>> m_known_files;
    };
}

#endif //KOVAAKSSTATSVIEWER_FILE_SERVICE_H
