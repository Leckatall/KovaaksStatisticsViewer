#ifndef KOVAAKSSTATSVIEWER_RUN_FILENAME_H
#define KOVAAKSSTATSVIEWER_RUN_FILENAME_H

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "interfaces/i_file_service.h"

namespace ksv::data {
    struct CsvTiming {
        long long start_time = 0;
        float scenario_length = 0.0F;
        bool crossed_midnight = false;
    };

    [[nodiscard]] std::string pairingStem(std::string_view filename);
    [[nodiscard]] application::StatsFile statsSibling(const application::PerfFile &perf);
    [[nodiscard]] std::optional<CsvTiming> deriveCsvTiming(
        std::string_view filename, std::chrono::milliseconds challenge_start);
}

#endif // KOVAAKSSTATSVIEWER_RUN_FILENAME_H
