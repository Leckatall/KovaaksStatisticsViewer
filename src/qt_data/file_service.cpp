//
// Created by Lecka on 01/08/2026.
//

#include "file_service.h"

#include <qdatetime.h>
#include <qdir.h>
#include <qfileinfo.h>

namespace ksv::qt_data {
    FileService::FileService(std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProtoDecoder> decoder, QObject *parent): QObject(parent),
m_settings_service(std::move(settings_service)), m_decoder(std::move(decoder)){
        watchPerfDir();
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString&) { handleDirectoryChanged(); });
        m_settings_service->onKovaaksDirChanged([this] { repointWatcher(); });
    }

    void FileService::watchPerfDir() {
        if (const auto perf_dir = get_perf_dir()) {
            m_watcher.addPath(perf_dir->absolutePath());
            const auto files = perf_dir->entryList(QDir::Files);
            m_known_files = QSet(files.begin(), files.end());
        }
    }

    void FileService::repointWatcher() {
        const auto watched = m_watcher.directories();
        if (!watched.isEmpty()) m_watcher.removePaths(watched);
        watchPerfDir();
    }

    void FileService::handleDirectoryChanged() {
        const auto perf_dir = get_perf_dir();
        if (!perf_dir) return;

        const auto files = perf_dir->entryList(QDir::Files);
        const QSet current_files(files.begin(), files.end());

        // TEMP DIAGNOSTIC (2026-08-10): logging file size/mtime per directoryChanged firing to
        // establish whether KovaaKs writes .perf files in place (race) or renames them in atomically.
        // Remove after the investigation in fileservice-handledirectorychanged-decod-serene-pancake.md.
        for (const auto &file : files) {
            const QFileInfo info(perf_dir->absoluteFilePath(file));
            qDebug() << "[perf-watch]" << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                      << file << "size=" << info.size() << "lastModified=" << info.lastModified()
                      << (m_known_files.contains(file) ? "known" : "NEW");
        }

        for (const auto &file : current_files) {
            if (!m_known_files.contains(file)) {
                notifyFilesChanged(perf_dir->absoluteFilePath(file).toStdString());
            }
        }
        m_known_files = current_files;
    }

    std::vector<domain::ScenarioPerf> FileService::getAllPerfsFromFiles() const {
        const auto perf_dir = get_perf_dir();
        if (!perf_dir) {
            qDebug() << "Could not cd to performances dir";
            return {};
        }
        auto files = perf_dir->entryList(QDir::Files);
        std::vector<domain::ScenarioPerf> perfs;
        for (const auto &file: files) {
            perfs.push_back(getPerfFromFile(perf_dir->absoluteFilePath(file).toStdString()));
        }
        return perfs;
    }

    domain::ScenarioPerf FileService::getPerfFromFile(const std::string_view filename) const {
        return m_decoder->decode_file(filename);
    }

    std::string FileService::getSourceDirectory() const {
        const auto perf_dir = get_perf_dir();
        if (!perf_dir) return {};
        return perf_dir->absolutePath().toStdString();
    }

    domain::ScenarioPerf FileService::getLatestPerf() const {
        // DEPRECATED: Access through profile service now
        const auto perf_dir = get_perf_dir();
        if (!perf_dir) {
            qDebug() << "Could not cd to performances dir";
            return {};
        }
        auto files = perf_dir->entryList(QDir::Files, QDir::Time);
        if (files.isEmpty()) return {};
        const auto latest_file = files.takeFirst();
        qDebug() << "Latest file: " << latest_file;
        return getPerfFromFile(perf_dir->absoluteFilePath(latest_file).toStdString());
    }
}
