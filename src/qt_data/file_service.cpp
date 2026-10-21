//
// Created by Lecka on 01/08/2026.
//

#include "file_service.h"

#include <algorithm>

#include <qdir.h>
#include <QDateTime>

namespace ksv::qt_data {
    FileService::FileService(std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProtoDecoder> decoder, std::shared_ptr<data::IStatsCsvParser> stats_parser, QObject *parent): QObject(parent),
m_settings_service(std::move(settings_service)), m_decoder(std::move(decoder)), m_stats_parser(std::move(stats_parser)){
        watchPerfDirs();
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(250);
        connect(&m_debounce, &QTimer::timeout, this, [this] { refreshWatches(true); });
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString &directory) { handleDirectoryChanged(directory); });
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
        const auto lifetime = std::weak_ptr(m_lifetime);
        m_settings_service->onKovaaksDirsChanged([this, lifetime] {
            if (!lifetime.expired()) repointWatcher();
        });
    }

    std::vector<FileService::PerfDir> FileService::perfDirs() const {
        std::vector<PerfDir> result;
        for (const auto &root : sourceRoots()) {
            QDir directory(QString::fromStdString(root));
            if (directory.cd("FPSAimTrainer/performances")) {
                result.push_back({root, "FPSAimTrainer/performances", directory});
            }
        }
        return result;
    }

    void FileService::watchPerfDirs() {
        refreshWatches(false);
    }

    void FileService::repointWatcher() {
        m_debounce.stop();
        const auto watched = m_watcher.directories() + m_watcher.files();
        if (!watched.isEmpty()) m_watcher.removePaths(watched);
        m_listing.clear();
        m_pending_fingerprints.clear();
        m_pending_sources.clear();
        watchPerfDirs();
    }

    void FileService::handleDirectoryChanged(const QString &) {
        m_debounce.start();
    }

    void FileService::refreshWatches(bool notify) {
        QStringList directories;
        for (const auto &root : sourceRoots()) {
            QDir directory(QString::fromStdString(root));
            while (!directory.exists() && !directory.isRoot()) {
                const auto parent = QFileInfo(directory.absolutePath()).absolutePath();
                if (parent == directory.absolutePath()) break;
                directory.setPath(parent);
            }
            if (directory.exists()) directories.push_back(directory.absolutePath());
            const QDir install(QString::fromStdString(root));
            for (const auto &subdir : {"FPSAimTrainer", "FPSAimTrainer/performances", "FPSAimTrainer/stats"}) {
                const QDir child(install.filePath(subdir));
                if (child.exists()) directories.push_back(child.absolutePath());
            }
        }
        const auto scan = scanSources(sourceRoots());
        QSet<QString> listing;
        for (const auto &perf : scan.perfs) listing.insert(QString::fromStdString(perf.absolutePath()));
        for (const auto &csv : scan.csvs) listing.insert(QString::fromStdString(csv.absolutePath()));
        if (notify) {
            for (const auto &path : listing) {
                if (!m_listing.contains(path)) m_pending_sources.insert(path);
            }
        }
        directories.removeDuplicates();
        for (auto it = m_pending_sources.begin(); it != m_pending_sources.end(); ) {
            if (!listing.contains(*it)) it = m_pending_sources.erase(it);
            else ++it;
        }
        // Only unacknowledged files can still grow in place; stat-ing every historical file would stall the GUI thread.
        bool pending_changed = false;
        QHash<QString, QPair<qint64, qint64>> pending_fingerprints;
        for (const auto &path : std::as_const(m_pending_sources)) {
            const auto fingerprint = fingerprintOf(path);
            const auto previous = m_pending_fingerprints.constFind(path);
            if (previous != m_pending_fingerprints.constEnd() && *previous != fingerprint) pending_changed = true;
            pending_fingerprints.insert(path, fingerprint);
        }
        m_pending_fingerprints = std::move(pending_fingerprints);
        QSet<QString> desired(directories.begin(), directories.end());
        desired.unite(m_pending_sources);
        const auto watched = m_watcher.directories() + m_watcher.files();
        const QSet<QString> watched_set(watched.begin(), watched.end());
        const auto remove = (watched_set - desired).values();
        const auto add = (desired - watched_set).values();
        if (!remove.isEmpty()) m_watcher.removePaths(remove);
        if (!add.isEmpty()) m_watcher.addPaths(add);
        const bool changed = m_listing != listing || pending_changed;
        m_listing = std::move(listing);
        if (notify && changed) {
            for (auto &callback : m_source_callbacks) callback();
        }
    }

    QPair<qint64, qint64> FileService::fingerprintOf(const QString &path) {
        const QFileInfo info(path);
        return {info.size(), info.lastModified().toMSecsSinceEpoch()};
    }

    void FileService::acknowledgeImport(const std::vector<std::string> &successful,
                                       const std::vector<std::string> &retryable) {
        QStringList remove;
        QStringList add;
        const auto watched = m_watcher.files();
        const QSet<QString> watched_set(watched.begin(), watched.end());
        for (const auto &path : successful) {
            const auto value = QString::fromStdString(path);
            m_pending_sources.remove(value);
            m_pending_fingerprints.remove(value);
            if (watched_set.contains(value)) remove.push_back(value);
        }
        for (const auto &path : retryable) {
            const auto value = QString::fromStdString(path);
            if (!QFileInfo(value).isFile()) continue;
            m_pending_sources.insert(value);
            m_pending_fingerprints.insert(value, fingerprintOf(value));
            if (!watched_set.contains(value)) add.push_back(value);
        }
        if (!remove.isEmpty()) m_watcher.removePaths(remove);
        if (!add.isEmpty()) m_watcher.addPaths(add);
    }

    std::vector<application::PerfFile> FileService::listPerfFiles() const {
        const auto perf_dirs = perfDirs();
        if (perf_dirs.empty()) {
            qDebug() << "Could not cd to performances dir";
            return {};
        }
        std::vector<application::PerfFile> paths;
        for (const auto &perf_dir : perf_dirs) {
            const auto files = perf_dir.directory.entryList(QDir::Files);
            paths.reserve(paths.size() + files.size());
            for (const auto &file: files) {
                paths.push_back({perf_dir.root, perf_dir.subdir, file.toStdString()});
            }
        }
        return paths;
    }

    std::vector<application::StatsFile> FileService::listStatsFiles() const {
        std::vector<application::StatsFile> paths;
        for (const auto &root : sourceRoots()) {
            QDir directory(QString::fromStdString(root));
            if (!directory.cd("FPSAimTrainer/stats")) continue;
            for (const auto &file : directory.entryList({"*.csv"}, QDir::Files)) {
                paths.push_back({root, "FPSAimTrainer/stats", file.toStdString()});
            }
        }
        return paths;
    }

    application::SourceScan FileService::scanSources(const std::vector<std::string> &roots) const {
        application::SourceScan scan;
        for (const auto &root : roots) {
            const QDir directory(QString::fromStdString(root));
            if (!directory.exists() || !QFileInfo(directory.absolutePath()).isReadable()) {
                scan.issues.push_back({root, "Source directory is unavailable or unreadable", true});
                continue;
            }
            const QDir perfs(directory.filePath("FPSAimTrainer/performances"));
            const QDir csvs(directory.filePath("FPSAimTrainer/stats"));
            for (const auto &file : perfs.entryList({"*.perf"}, QDir::Files))
                scan.perfs.push_back({root, "FPSAimTrainer/performances", file.toStdString()});
            for (const auto &file : csvs.entryList({"*.csv"}, QDir::Files))
                scan.csvs.push_back({root, "FPSAimTrainer/stats", file.toStdString()});
            for (const auto &subdir : {perfs, csvs}) {
                const QFileInfo info(subdir.absolutePath());
                if (info.exists() && !info.isReadable())
                    scan.issues.push_back({subdir.absolutePath().toStdString(), "Source directory is unreadable", true});
            }
        }
        return scan;
    }

    domain::Run FileService::getPerfFromFile(const std::string_view filename) const {
        return m_decoder->decode_file(filename);
    }

    std::optional<data::ParsedStatsCsv> FileService::getStatsFromFile(const std::filesystem::path &path) const {
        return m_stats_parser->parseFile(path);
    }

    std::vector<std::string> FileService::sourceRoots() const {
        return m_settings_service->getKovaaksDirs();
    }

}
