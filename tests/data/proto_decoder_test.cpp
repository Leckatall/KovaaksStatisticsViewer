//
// Created by Lecka on 30/07/2026.
//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "formats/protobuf/proto_decoder.h"

namespace {
    class ProtoDecoderTest : public testing::Test {
    protected:
        ksv::data::ProtoDecoder decoder;

        static std::string get_test_file_path() {
            return (std::filesystem::path{TEST_FILES_DIR} / "1wall6targets TE.perf").string();
        }
        static perf::PerfLog make_perf_log() {
            perf::PerfLog log;

            auto *meta = log.mutable_meta();
            meta->set_scenario_name("testing");
            meta->set_timestamp_ms(0LL);
            meta->mutable_details()->set_scenario_length(69.0F);

            return log;
        }
    };

    TEST_F(ProtoDecoderTest, decode) {
        auto perf = decoder.decode(make_perf_log());
        perf.print();
        EXPECT_EQ(perf.scenario_name, "testing");
        EXPECT_EQ(perf.start_time, 0LL);
        EXPECT_EQ(perf.scenario_length, 69.0F);
    }

    TEST_F(ProtoDecoderTest, decode_file) {
        auto perf = decoder.decode_file(get_test_file_path());
        perf.print();
        EXPECT_EQ(perf.scenario_name, "1wall6targets TE");
        EXPECT_EQ(perf.start_time, 1783733140000LL);
        EXPECT_EQ(perf.scenario_length, 60.0F);
    }
}
