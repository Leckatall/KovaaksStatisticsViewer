//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_FILE_SERVICE_H

#include <optional>
#include <qdir.h>
#include <qfilesystemwatcher.h>
#include <QObject>
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

        // [[nodiscard]] std::vector<std::string> get_files_in_dir(std::string_view dir) const;

        [[nodiscard]] QDir get_kovaaks_dir() const {
            return {m_settings_service->getKovaaksDir().data()};
        }

        [[nodiscard]] std::optional<QDir> get_perf_dir() const {
            QDir perf_dir(get_kovaaks_dir());
            if (!perf_dir.cd("FPSAimTrainer/performances")) return std::nullopt;
            return perf_dir;
        }

        [[nodiscard]] std::vector<domain::ScenarioPerf> getAllPerfsFromFiles() const override;
        [[nodiscard]] domain::ScenarioPerf getPerfFromFile(std::string_view filename) const override;
        [[nodiscard]] domain::ScenarioPerf getLatestPerf() const override;
        [[nodiscard]] std::string getSourceDirectory() const override;

        void onFilesChanged(std::function<void(const std::string& path)> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        void notifyFilesChanged(const std::string& path) const {
            for (auto& cb : m_callbacks) cb(path);
        }

        // QFileSystemWatcher::directoryChanged only reports the watched directory's own
        // path, not which file changed inside it, so new files are found by diffing
        // entryList() snapshots taken before and after the signal fires.
        void handleDirectoryChanged();

        void watchPerfDir();
        void repointWatcher();

        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProtoDecoder> m_decoder;

        QFileSystemWatcher m_watcher;
        std::vector<std::function<void(const std::string& path)>> m_callbacks;
        QSet<QString> m_known_files;
    };
}

#endif //KOVAAKSSTATSVIEWER_FILE_SERVICE_H
