//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H

#include <vector>
#include <functional>

#include "scenario_perf.h"

namespace ksv::application {
    class IFileService {
        public:
        virtual ~IFileService() = default;
        // Absolute paths, undecoded — a caller that decodes them one at a time can
        // report progress, which getAllPerfsFromFiles() could not.
        [[nodiscard]] virtual std::vector<std::string> listPerfFiles() const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getPerfFromFile(std::string_view filename) const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getLatestPerf() const = 0;
        [[nodiscard]] virtual std::string getSourceDirectory() const = 0;
        virtual void onFilesChanged(std::function<void(const std::string& path)> callback) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
