#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_FILE_SERVICE_H

#include <QSemaphore>

#include <filesystem>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "data/interfaces/i_file_service.h"

namespace ksv::tests_support {
    class FakeFileService final : public application::IFileService {
    public:
        std::vector<domain::Run> perfs_to_return;
        std::vector<application::StatsFile> stats_files;
        std::unordered_map<std::string, domain::Run> perfs_by_path;
        std::unordered_map<std::string, data::ParsedStatsCsv> stats_by_path;
        std::set<std::size_t> throw_for_indices;
        std::set<std::string> paths_to_throw_for;
        std::vector<std::string> source_roots{"fake/kovaaks"};
        bool gate_scan = false;
        QSemaphore scan_gate;
        QSemaphore scan_entered;

        [[nodiscard]] std::vector<application::PerfFile> listPerfFiles() const override {
            if (gate_scan) {
                const_cast<QSemaphore &>(scan_entered).release();
                const_cast<QSemaphore &>(scan_gate).acquire();
            }
            std::vector<application::PerfFile> paths;
            for (std::size_t i = 0; i < perfs_to_return.size(); ++i) {
                paths.push_back({source_roots.front(), "FPSAimTrainer/performances", "listed-perf-" + std::to_string(i)});
            }
            return paths;
        }

        [[nodiscard]] domain::Run getPerfFromFile(const std::string_view filename) const override {
            const std::string path(filename);
            if (paths_to_throw_for.contains(path)) throw std::invalid_argument("File does not exist");
            if (const auto it = perfs_by_path.find(path); it != perfs_by_path.end()) return it->second;
            const auto name = std::filesystem::path(path).filename().string();
            if (const auto it = perfs_by_path.find(name); it != perfs_by_path.end()) return it->second;
            const auto index = std::stoul(name.substr(std::string("listed-perf-").size()));
            if (throw_for_indices.contains(index)) throw std::invalid_argument("File does not exist");
            return perfs_to_return.at(index);
        }

        [[nodiscard]] std::vector<application::StatsFile> listStatsFiles() const override { return stats_files; }

        [[nodiscard]] std::optional<data::ParsedStatsCsv> getStatsFromFile(
            const std::filesystem::path &path) const override {
            if (const auto it = stats_by_path.find(path.generic_string()); it != stats_by_path.end()) return it->second;
            if (const auto it = stats_by_path.find(path.filename().string()); it != stats_by_path.end()) return it->second;
            return std::nullopt;
        }

        [[nodiscard]] std::vector<std::string> sourceRoots() const override { return source_roots; }

        void onFilesChanged(std::function<void(const application::PerfFile &)> callback) override {
            callbacks.push_back(std::move(callback));
        }

        void notifyFilesChanged(const application::PerfFile &file) const {
            for (const auto &callback: callbacks) callback(file);
        }

        [[nodiscard]] std::size_t callbackCount() const { return callbacks.size(); }

    private:
        std::vector<std::function<void(const application::PerfFile &)>> callbacks;
    };
}

#endif
