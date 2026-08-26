//
// Created by Lecka on 01/08/2026.
//

#include "file_service.h"

#include <algorithm>

#include <qdir.h>

namespace ksv::qt_data {
    FileService::FileService(std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProtoDecoder> decoder, QObject *parent): QObject(parent),
m_settings_service(std::move(settings_service)), m_decoder(std::move(decoder)){
        watchPerfDirs();
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString &directory) { handleDirectoryChanged(directory); });
        m_settings_service->onKovaaksDirsChanged([this] { repointWatcher(); });
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
        for (const auto &perf_dir : perfDirs()) {
            const auto path = perf_dir.directory.absolutePath();
            m_watcher.addPath(path);
            const auto files = perf_dir.directory.entryList(QDir::Files);
            m_known_files.insert(path, QSet(files.begin(), files.end()));
        }
    }

    void FileService::repointWatcher() {
        const auto watched = m_watcher.directories();
        if (!watched.isEmpty()) m_watcher.removePaths(watched);
        m_known_files.clear();
        watchPerfDirs();
    }

    void FileService::handleDirectoryChanged(const QString &directory) {
        const auto perf_dirs = perfDirs();
        const auto perf_dir = std::ranges::find_if(perf_dirs, [&](const PerfDir &candidate) {
            return candidate.directory.absolutePath() == QDir(directory).absolutePath();
        });
        if (perf_dir == perf_dirs.end()) return;

        const auto files = perf_dir->directory.entryList(QDir::Files);
        const QSet current_files(files.begin(), files.end());
        const auto known_files = m_known_files.value(perf_dir->directory.absolutePath());

        for (const auto &file : current_files) {
            if (!known_files.contains(file)) {
                notifyFilesChanged({perf_dir->root, perf_dir->subdir, file.toStdString()});
            }
        }
        m_known_files.insert(perf_dir->directory.absolutePath(), current_files);
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

    domain::ScenarioPerf FileService::getPerfFromFile(const std::string_view filename) const {
        return m_decoder->decode_file(filename);
    }

    std::vector<std::string> FileService::sourceRoots() const {
        return m_settings_service->getKovaaksDirs();
    }

}
