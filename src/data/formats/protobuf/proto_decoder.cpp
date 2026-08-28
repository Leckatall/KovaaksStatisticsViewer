//
// Created by Lecka on 29/07/2026.
//

#include "proto_decoder.h"

namespace ksv::data {
    domain::Run ProtoDecoder::decode(const perf::PerfLog &perfLog) {
        domain::Run run;
        run.run_id.scenario_id.name = perfLog.meta().scenario_name();
        run.run_id.scenario_id.hash = perfLog.meta().scenario_hash();
        run.run_id.start_time = perfLog.meta().timestamp_ms();
        run.scenario_length = perfLog.meta().details().scenario_length();
        run.performance.emplace();

        for (const auto &entry: perfLog.entries()) {
            if (entry.has_shots()) run.performance->add_data(entry.time(), domain::DataPointType::SHOTS, entry.shots().value());
            if (entry.has_hits()) run.performance->add_data(entry.time(), domain::DataPointType::HITS, entry.hits().value());
            if (entry.has_misses()) run.performance->add_data(entry.time(), domain::DataPointType::MISSES, entry.misses().value());
            if (entry.has_dmg()) run.performance->add_data(entry.time(), domain::DataPointType::DMG, entry.dmg().value());
            if (entry.has_dmg_possible()) run.performance->add_data(entry.time(), domain::DataPointType::DMG_POSSIBLE, entry.dmg_possible().value());
            if (entry.has_score()) run.performance->add_data(entry.time(), domain::DataPointType::SCORE, entry.score().value());
            if (entry.has_kills()) run.performance->add_data(entry.time(), domain::DataPointType::KILLS, entry.kills().value());
        }
        for (const auto &sample : run.performance->samples) {
            run.stored_totals.score += sample.score;
            run.stored_totals.shots += sample.shots;
            run.stored_totals.hits += sample.hits;
            run.stored_totals.misses += sample.misses;
            run.stored_totals.kills += sample.kills;
        }
        return run;
    }

    domain::Run ProtoDecoder::decode_file(const std::string_view filename) {
        GOOGLE_PROTOBUF_VERIFY_VERSION;
        perf::PerfLog perfLog;
        if (!std::filesystem::exists(filename)) throw std::invalid_argument("File does not exist");
        std::fstream input(filename.data(), std::ios::in | std::ios::binary);
        if (!perfLog.ParseFromIstream(&input)) {
            std::cerr << "Failed to parse file." << std::endl;
        }
        return decode(perfLog);
    }

}
