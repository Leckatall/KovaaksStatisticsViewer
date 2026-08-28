//
// Created by Lecka on 29/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROTO_DECODER_H
#define KOVAAKSSTATSVIEWER_PROTO_DECODER_H

#include "formats/protobuf/schema/perf.pb.h"
#include <fstream>

#include <run.h>

#include "interfaces/i_proto_decoder.h"

namespace ksv::data {
    class ProtoDecoder: public application::IProtoDecoder {
    public:
        domain::Run decode(const perf::PerfLog& perfLog) override;
        domain::Run decode_file(std::string_view filename) override;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROTO_DECODER_H
