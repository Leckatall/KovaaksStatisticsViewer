//
// Created by Lecka on 29/07/2026.
//

#include "proto_decoder.h"

namespace ksv::data {
    domain::ScenarioPerf ProtoDecoder::decode(const perf::PerfLog &perfLog) {
        domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = perfLog.meta().scenario_name();
        perf.run_id.scenario_id.hash = perfLog.meta().scenario_hash();
        perf.run_id.start_time = perfLog.meta().timestamp_ms();
        perf.scenario_length = perfLog.meta().details().scenario_length();

        for (const auto &entry: perfLog.entries()) {
            if (entry.has_shots()) perf.add_data(entry.time(), domain::DataPointType::SHOTS, entry.shots().value());
            if (entry.has_hits()) perf.add_data(entry.time(), domain::DataPointType::HITS, entry.hits().value());
            if (entry.has_misses()) perf.add_data(entry.time(), domain::DataPointType::MISSES, entry.misses().value());
            if (entry.has_dmg()) perf.add_data(entry.time(), domain::DataPointType::DMG, entry.dmg().value());
            if (entry.has_dmg_possible()) perf.add_data(entry.time(), domain::DataPointType::DMG_POSSIBLE, entry.dmg_possible().value());
            if (entry.has_score()) perf.add_data(entry.time(), domain::DataPointType::SCORE, entry.score().value());
            if (entry.has_kills()) perf.add_data(entry.time(), domain::DataPointType::KILLS, entry.kills().value());
        }
        return perf;
    }

    domain::ScenarioPerf ProtoDecoder::decode_file(const std::string_view filename) {
        GOOGLE_PROTOBUF_VERIFY_VERSION;
        perf::PerfLog perfLog;
        if (!std::filesystem::exists(filename)) throw std::invalid_argument("File does not exist");
        std::fstream input(filename.data(), std::ios::in | std::ios::binary);
        if (!perfLog.ParseFromIstream(&input)) {
            std::cerr << "Failed to parse file." << std::endl;
        }
        auto perf = decode(perfLog);
        perf.source_file = filename;
        return perf;
    }

}