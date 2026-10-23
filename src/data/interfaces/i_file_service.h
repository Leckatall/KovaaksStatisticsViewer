//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H

#include <cstdint>
#include <vector>
#include <functional>
#include <filesystem>

#include "run.h"
#include "app/contracts/profile_import.h"
#include "data/interfaces/i_stats_csv_parser.h"

namespace ksv::application {
    struct PerfFile {
        std::string root;
        std::string subdir;
        std::string filename;
        std::int64_t size = -1;
        std::int64_t modified_ms = -1;

        [[nodiscard]] std::string absolutePath() const {
            return (std::filesystem::path(root) / subdir / filename).generic_string();
        }

        bool operator==(const PerfFile &) const = default;
    };

    struct StatsFile {
        std::string root;
        std::string subdir;
        std::string filename;
        std::int64_t size = -1;
        std::int64_t modified_ms = -1;

        [[nodiscard]] std::string absolutePath() const {
            return (std::filesystem::path(root) / subdir / filename).generic_string();
        }

        bool operator==(const StatsFile &) const = default;
    };

    struct SourceScan {
        std::vector<PerfFile> perfs;
        std::vector<StatsFile> csvs;
        std::vector<ProfileImportIssue> issues;
    };

    class IFileService {
        public:
        virtual ~IFileService() = default;
        // Absolute paths, undecoded — a caller that decodes them one at a time can
        // report progress, which getAllPerfsFromFiles() could not.
        [[nodiscard]] virtual std::vector<PerfFile> listPerfFiles() const = 0;
        [[nodiscard]] virtual std::vector<StatsFile> listStatsFiles() const = 0;
        [[nodiscard]] virtual SourceScan scanSources(const std::vector<std::string> &) const {
            return {listPerfFiles(), listStatsFiles(), {}};
        }
        [[nodiscard]] virtual domain::Run getPerfFromFile(std::string_view filename) const = 0;
        [[nodiscard]] virtual std::optional<data::ParsedStatsCsv> getStatsFromFile(
            const std::filesystem::path &path) const = 0;
        [[nodiscard]] virtual std::vector<std::string> sourceRoots() const = 0;
        // True when there is at least one root and every root holds a KovaaK's performances or stats directory.
        [[nodiscard]] virtual bool hasValidSourceRoots() const = 0;
        virtual void onSourcesChanged(std::function<void()> callback) = 0;
        virtual void acknowledgeImport(const std::vector<std::string> &, const std::vector<std::string> &) {}
    };
}
#endif //KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
