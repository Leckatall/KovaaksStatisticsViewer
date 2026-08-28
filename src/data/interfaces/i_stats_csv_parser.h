#ifndef KOVAAKSSTATSVIEWER_I_STATS_CSV_PARSER_H
#define KOVAAKSSTATSVIEWER_I_STATS_CSV_PARSER_H

#include <chrono>
#include <filesystem>
#include <optional>

#include "run.h"

namespace ksv::data {
    struct ParsedStatsCsv {
        domain::ScenarioId scenario_id;
        domain::RunTotals totals;
        domain::Stats stats;
        std::chrono::milliseconds challenge_start;
        int pause_count = 0;
        std::chrono::milliseconds pause_duration;
    };

    class IStatsCsvParser {
    public:
        virtual ~IStatsCsvParser() = default;
        [[nodiscard]] virtual std::optional<ParsedStatsCsv> parseFile(const std::filesystem::path &path) const = 0;
    };
}

#endif // KOVAAKSSTATSVIEWER_I_STATS_CSV_PARSER_H
