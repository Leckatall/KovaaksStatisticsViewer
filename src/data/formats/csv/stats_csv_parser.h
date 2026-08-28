#ifndef KOVAAKSSTATSVIEWER_STATS_CSV_PARSER_H
#define KOVAAKSSTATSVIEWER_STATS_CSV_PARSER_H

#include "interfaces/i_stats_csv_parser.h"

namespace ksv::data {
    class StatsCsvParser final : public IStatsCsvParser {
    public:
        [[nodiscard]] std::optional<ParsedStatsCsv> parseFile(const std::filesystem::path &path) const override;
    };
}

#endif // KOVAAKSSTATSVIEWER_STATS_CSV_PARSER_H
