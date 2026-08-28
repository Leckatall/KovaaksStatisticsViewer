//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H

#include <vector>
#include <functional>
#include <filesystem>

#include "run.h"
#include "interfaces/i_stats_csv_parser.h"

namespace ksv::application {
    struct PerfFile {
        std::string root;
        std::string subdir;
        std::string filename;

        [[nodiscard]] std::string absolutePath() const {
            return (std::filesystem::path(root) / subdir / filename).generic_string();
        }

        bool operator==(const PerfFile &) const = default;
    };

    struct StatsFile {
        std::string root;
        std::string subdir;
        std::string filename;

        [[nodiscard]] std::string absolutePath() const {
            return (std::filesystem::path(root) / subdir / filename).generic_string();
        }

        bool operator==(const StatsFile &) const = default;
    };

    class IFileService {
        public:
        virtual ~IFileService() = default;
        // Absolute paths, undecoded — a caller that decodes them one at a time can
        // report progress, which getAllPerfsFromFiles() could not.
        [[nodiscard]] virtual std::vector<PerfFile> listPerfFiles() const = 0;
        [[nodiscard]] virtual std::vector<StatsFile> listStatsFiles() const = 0;
        [[nodiscard]] virtual domain::Run getPerfFromFile(std::string_view filename) const = 0;
        [[nodiscard]] virtual std::optional<data::ParsedStatsCsv> getStatsFromFile(
            const std::filesystem::path &path) const = 0;
        [[nodiscard]] virtual std::vector<std::string> sourceRoots() const = 0;
        virtual void onFilesChanged(std::function<void(const PerfFile &)> callback) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
