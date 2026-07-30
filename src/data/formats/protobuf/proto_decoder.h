//
// Created by Lecka on 29/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROTO_DECODER_H
#define KOVAAKSSTATSVIEWER_PROTO_DECODER_H

#include "formats/protobuf/schema/perf.pb.h"
#include <fstream>

#include <scenario_perf.h>

namespace ksv::data {
    class ProtoDecoder {
    public:
        // ProtoDecoder();
        domain::ScenarioPerf decode(const perf::PerfLog& perfLog);
        domain::ScenarioPerf decode_file(const std::string& filename);
    };
} // ksv

#endif //KOVAAKSSTATSVIEWER_PROTO_DECODER_H
