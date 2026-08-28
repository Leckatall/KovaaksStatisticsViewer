#include "run_filename.h"

#include <charconv>
#include <ctime>

namespace ksv::data {
    namespace {
        constexpr std::string_view kPerfSuffix = " Performance.perf";
        constexpr std::string_view kCsvSuffix = " Stats.csv";

        bool parseInt(const std::string_view value, int &result) {
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
            return error == std::errc{} && end == value.end();
        }

        std::optional<std::tm> endTimeFrom(std::string_view filename) {
            const auto separator = filename.rfind(" - ");
            if (separator == std::string_view::npos) return std::nullopt;
            const auto timestamp = filename.substr(separator + 3);
            if (timestamp.size() != 19 || timestamp[4] != '.' || timestamp[7] != '.' || timestamp[10] != '-' ||
                timestamp[13] != '.' || timestamp[16] != '.') return std::nullopt;

            int year, month, day, hour, minute, second;
            if (!parseInt(timestamp.substr(0, 4), year) || !parseInt(timestamp.substr(5, 2), month) ||
                !parseInt(timestamp.substr(8, 2), day) || !parseInt(timestamp.substr(11, 2), hour) ||
                !parseInt(timestamp.substr(14, 2), minute) || !parseInt(timestamp.substr(17, 2), second)) {
                return std::nullopt;
            }
            std::tm result{};
            result.tm_year = year - 1900;
            result.tm_mon = month - 1;
            result.tm_mday = day;
            result.tm_hour = hour;
            result.tm_min = minute;
            result.tm_sec = second;
            result.tm_isdst = -1;
            return result;
        }
    }

    std::string pairingStem(const std::string_view filename) {
        if (filename.ends_with(kPerfSuffix)) return std::string{filename.substr(0, filename.size() - kPerfSuffix.size())};
        if (filename.ends_with(kCsvSuffix)) return std::string{filename.substr(0, filename.size() - kCsvSuffix.size())};
        const auto extension = filename.rfind('.');
        return extension == std::string_view::npos ? std::string{filename} : std::string{filename.substr(0, extension)};
    }

    application::StatsFile statsSibling(const application::PerfFile &perf) {
        auto subdir = perf.subdir;
        const auto performances = subdir.rfind("performances");
        if (performances != std::string::npos) subdir.replace(performances, std::string_view{"performances"}.size(), "stats");
        return {.root = perf.root, .subdir = std::move(subdir), .filename = pairingStem(perf.filename) + " Stats.csv"};
    }

    std::optional<CsvTiming> deriveCsvTiming(const std::string_view filename,
                                               const std::chrono::milliseconds challenge_start) {
        auto end_tm = endTimeFrom(pairingStem(filename));
        if (!end_tm || challenge_start.count() < 0 || challenge_start >= std::chrono::days{1}) return std::nullopt;

        const auto end_time = std::mktime(&*end_tm);
        if (end_time == static_cast<std::time_t>(-1)) return std::nullopt;

        using namespace std::chrono;
        const auto end_clock = hours{end_tm->tm_hour} + minutes{end_tm->tm_min} + seconds{end_tm->tm_sec};
        // The filename timestamp is whole-second, so a sub-second apparent deficit is
        // truncation noise, not a real midnight rollback.
        const bool crossed_midnight = challenge_start > end_clock + seconds{1};
        auto span = end_clock - challenge_start + (crossed_midnight ? days{1} : days{0});
        if (span.count() < 0) span = span.zero();
        const auto length = duration_cast<duration<float>>(span);
        const auto start = system_clock::from_time_t(end_time) - duration_cast<system_clock::duration>(length);
        return CsvTiming{
            .start_time = duration_cast<milliseconds>(start.time_since_epoch()).count(),
            .scenario_length = length.count(),
            .crossed_midnight = crossed_midnight,
        };
    }
}
