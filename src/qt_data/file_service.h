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
#include <QTimer>

#include "user_profile.h"
#include "app/usecases/i_session_controller.h"
#include "data/interfaces/i_proto_decoder.h"
#include "data/interfaces/i_stats_csv_parser.h"
#include "data/formats/csv/stats_csv_parser.h"
#include "data/interfaces/i_file_service.h"
#include "interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class FileService : public QObject, public application::IFileService {
        Q_OBJECT

    public:
        explicit FileService(std::shared_ptr<application::ISettingsService> settings_service, std::shared_ptr<application::IProtoDecoder> decoder, std::shared_ptr<data::IStatsCsvParser> stats_parser = std::make_shared<data::StatsCsvParser>(), QObject *parent = nullptr);

        [[nodiscard]] std::vector<application::PerfFile> listPerfFiles() const override;
        [[nodiscard]] std::vector<application::StatsFile> listStatsFiles() const override;
        [[nodiscard]] application::SourceScan scanSources(const std::vector<std::string> &roots) const override;
        [[nodiscard]] domain::Run getPerfFromFile(std::string_view filename) const override;
        [[nodiscard]] std::optional<data::ParsedStatsCsv> getStatsFromFile(const std::filesystem::path &path) const override;
        [[nodiscard]] std::vector<std::string> sourceRoots() const override;

        void onSourcesChanged(std::function<void()> callback) override {
            m_source_callbacks.push_back(std::move(callback));
        }
        void acknowledgeImport(const std::vector<std::string> &successful, const std::vector<std::string> &retryable) override;

    private:
        struct PerfDir {
            std::string root;
            std::string subdir;
            QDir directory;
        };

        [[nodiscard]] std::vector<PerfDir> perfDirs() const;

        [[nodiscard]] static QPair<qint64, qint64> fingerprintOf(const QString &path);

        void handleDirectoryChanged(const QString &directory);

        void watchPerfDirs();
        void repointWatcher();
        void refreshWatches(bool notify);

        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProtoDecoder> m_decoder;
        std::shared_ptr<data::IStatsCsvParser> m_stats_parser;

        QFileSystemWatcher m_watcher;
        QSet<QString> m_listing;
        QHash<QString, QPair<qint64, qint64>> m_pending_fingerprints;
        QSet<QString> m_pending_sources;
        std::vector<std::function<void()>> m_source_callbacks;
        QTimer m_debounce;
        std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    };
}

#endif //KOVAAKSSTATSVIEWER_FILE_SERVICE_H
