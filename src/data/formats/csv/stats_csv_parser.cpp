#include "stats_csv_parser.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <string>
#include <string_view>

namespace ksv::data {
    namespace {
        std::string_view trim(const std::string_view value) {
            constexpr auto whitespace = " \t\r\n";
            const auto first = value.find_first_not_of(whitespace);
            if (first == std::string_view::npos) return {};
            return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
        }

        std::optional<int> parseInt(const std::string_view value) {
            int parsed = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.end()) return std::nullopt;
            return parsed;
        }

        std::optional<float> parseFloat(const std::string_view value) {
            float parsed = 0.0F;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.end() || !std::isfinite(parsed)) return std::nullopt;
            return parsed;
        }

        std::optional<std::chrono::milliseconds> parseTimeOfDay(const std::string_view value) {
            const auto first_colon = value.find(':');
            if (first_colon == std::string_view::npos) return std::nullopt;
            const auto second_colon = value.find(':', first_colon + 1);
            if (second_colon == std::string_view::npos) return std::nullopt;
            // The fractional-second part is optional; older CSV variants omit it entirely.
            const auto decimal = value.find('.', second_colon + 1);
            const auto seconds_end = decimal == std::string_view::npos ? value.size() : decimal;

            const auto hour_value = parseInt(value.substr(0, first_colon));
            const auto minute_value = parseInt(value.substr(first_colon + 1, second_colon - first_colon - 1));
            const auto second_value = parseInt(value.substr(second_colon + 1, seconds_end - second_colon - 1));
            if (!hour_value || !minute_value || !second_value || *hour_value < 0 || *hour_value >= 24 ||
                *minute_value < 0 || *minute_value >= 60 || *second_value < 0 || *second_value >= 60) return std::nullopt;

            int millisecond_value = 0;
            if (decimal != std::string_view::npos) {
                auto fraction = value.substr(decimal + 1);
                if (fraction.size() > 3) fraction = fraction.substr(0, 3);
                const auto digits = parseInt(fraction);
                if (!digits || *digits < 0) return std::nullopt;
                millisecond_value = *digits;
                for (auto width = fraction.size(); width < 3; ++width) millisecond_value *= 10;
            }

            using namespace std::chrono;
            return duration_cast<milliseconds>(hours{*hour_value} + minutes{*minute_value} + seconds{*second_value}) +
                   milliseconds{millisecond_value};
        }
    }

    std::optional<ParsedStatsCsv> StatsCsvParser::parseFile(const std::filesystem::path &path) const {
        std::ifstream input(path);
        if (!input) return std::nullopt;

        ParsedStatsCsv parsed;
        // The per-kill rows and the weapon summary precede the footer; only the footer
        // rows are `Key:,value`. Start collecting at the first colon-terminated key that
        // follows a blank line, so the weapon summary is never parsed as footer data.
        bool footer_started = false;
        bool seen_blank = false;
        bool has_scenario = false;
        bool has_hash = false;
        bool has_start = false;
        bool has_score = false;
        bool has_hits = false;
        bool has_misses = false;
        bool has_kills = false;
        std::string line;
        while (std::getline(input, line)) {
            const auto trimmed_line = trim(line);
            const auto separator = trimmed_line.find(',');
            if (!footer_started) {
                if (trimmed_line.empty()) { seen_blank = true; continue; }
                if (!seen_blank || separator == std::string_view::npos) continue;
                if (!trim(trimmed_line.substr(0, separator)).ends_with(':')) continue;
                footer_started = true;
            } else if (trimmed_line.empty() || separator == std::string_view::npos) {
                continue;
            }

            auto key = trim(trimmed_line.substr(0, separator));
            const auto value = trim(trimmed_line.substr(separator + 1));
            if (key.ends_with(':')) key.remove_suffix(1);

            if (key == "Scenario") {
                parsed.scenario_id.name = value;
                has_scenario = !value.empty();
            } else if (key == "Hash") {
                parsed.scenario_id.hash = value;
                has_hash = !value.empty();
            } else if (key == "Challenge Start") {
                const auto parsed_time = parseTimeOfDay(value);
                if (!parsed_time) return std::nullopt;
                parsed.challenge_start = *parsed_time;
                has_start = true;
            } else if (key == "Score") {
                const auto score = parseFloat(value);
                if (!score) return std::nullopt;
                parsed.totals.score = *score;
                has_score = true;
            } else if (key == "Hit Count") {
                const auto hits = parseInt(value);
                if (!hits) return std::nullopt;
                parsed.totals.hits = *hits;
                has_hits = true;
            } else if (key == "Miss Count") {
                const auto misses = parseInt(value);
                if (!misses) return std::nullopt;
                parsed.totals.misses = *misses;
                has_misses = true;
            } else if (key == "Kills") {
                const auto kills = parseInt(value);
                if (!kills) return std::nullopt;
                parsed.totals.kills = *kills;
                has_kills = true;
            } else if (key == "Pause Count") {
                // Non-critical: an unparseable value is ignored, not fatal, so older
                // CSV variants without every field stay ingestible.
                if (const auto count = parseInt(value)) parsed.pause_count = *count;
            } else if (key == "Pause Duration") {
                if (const auto duration = parseFloat(value)) {
                    parsed.pause_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::duration<float>{*duration});
                }
            } else if (key == "Sens Scale") {
                parsed.stats.sens_scale = value;
            } else if (key == "Horiz Sens") {
                if (const auto sens = parseFloat(value)) parsed.stats.horiz_sens = *sens;
            } else if (key == "Vert Sens") {
                if (const auto sens = parseFloat(value)) parsed.stats.vert_sens = *sens;
            } else if (key == "DPI") {
                if (const auto dpi = parseInt(value)) parsed.stats.dpi = *dpi;
            } else if (key == "FOV") {
                if (const auto fov = parseFloat(value)) parsed.stats.fov = *fov;
            } else if (key == "FOVScale") {
                parsed.stats.fov_scale = value;
            } else if (key == "Resolution") {
                parsed.stats.resolution = value;
            } else if (key == "Resolution Scale") {
                if (const auto scale = parseFloat(value)) parsed.stats.resolution_scale = *scale;
            } else if (key == "Avg FPS") {
                if (const auto fps = parseFloat(value)) parsed.stats.avg_fps = *fps;
            }
        }

        if (!has_scenario || !has_hash || !has_start || !has_score || !has_hits || !has_misses || !has_kills) {
            return std::nullopt;
        }
        parsed.totals.shots = parsed.totals.hits + parsed.totals.misses;
        return parsed;
    }
}
