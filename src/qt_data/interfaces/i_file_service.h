//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
#include <vector>

#include "scenario_perf.h"

namespace ksv::application {
    class IFileService {
        public:
        virtual ~IFileService() = default;
        [[nodiscard]] virtual std::vector<domain::ScenarioPerf> getAllPerfsFromFiles() const = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_FILE_SERVICE_H
