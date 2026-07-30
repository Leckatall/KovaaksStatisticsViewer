//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROTO_DECODER_H
#define KOVAAKSSTATSVIEWER_I_PROTO_DECODER_H
#include "scenario_perf.h"
#include "formats/protobuf/schema/perf.pb.h"

namespace ksv::application {
    class IProtoDecoder {
    public:
        virtual ~IProtoDecoder() = default;
        virtual domain::ScenarioPerf decode(const perf::PerfLog &perfLog) = 0;
        virtual domain::ScenarioPerf decode_file(std::string_view filename) = 0;
    };
}


#endif //KOVAAKSSTATSVIEWER_I_PROTO_DECODER_H
