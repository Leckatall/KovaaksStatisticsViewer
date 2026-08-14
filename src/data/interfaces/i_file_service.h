//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H

#include <vector>
#include <functional>
#include <filesystem>

#include "scenario_perf.h"

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

    class IFileService {
        public:
        virtual ~IFileService() = default;
        // Absolute paths, undecoded — a caller that decodes them one at a time can
        // report progress, which getAllPerfsFromFiles() could not.
        [[nodiscard]] virtual std::vector<PerfFile> listPerfFiles() const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getPerfFromFile(std::string_view filename) const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getLatestPerf() const = 0;
        [[nodiscard]] virtual std::vector<std::string> sourceRoots() const = 0;
        virtual void onFilesChanged(std::function<void(const PerfFile &)> callback) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
