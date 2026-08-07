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
        EXPECT_EQ(perf.run_id.scenario_id.name, "testing");
        EXPECT_EQ(perf.run_id.start_time, 0LL);
        EXPECT_EQ(perf.scenario_length, 69.0F);
    }

    TEST_F(ProtoDecoderTest, decode_file) {
        auto perf = decoder.decode_file(get_test_file_path());
        perf.print();
        EXPECT_EQ(perf.run_id.scenario_id.name, "1wall6targets TE");
        EXPECT_EQ(perf.run_id.start_time, 1783733140000LL);
        EXPECT_EQ(perf.scenario_length, 60.0F);
        EXPECT_EQ(perf.source_file, get_test_file_path());
    }

    TEST_F(ProtoDecoderTest, decodeDoesNotSetSourceFile) {
        // The meta-only decode() overload has no filename in scope, so source_file
        // must stay empty there (only decode_file() knows the path).
        const auto perf = decoder.decode(make_perf_log());
        EXPECT_TRUE(perf.source_file.empty());
    }

    TEST_F(ProtoDecoderTest, decodeSkipsUnsetOptionalFields) {
        perf::PerfLog log = make_perf_log();
        auto *entry = log.add_entries();
        entry->set_time(1.0F);
        entry->mutable_shots()->set_value(10);
        // hits/misses/dmg/dmg_possible/score/kills intentionally left unset.

        const auto perf = decoder.decode(log);

        ASSERT_EQ(perf.data.size(), 1);
        EXPECT_EQ(perf.data[0].shots, 10);
        EXPECT_EQ(perf.data[0].hits, 0);
        EXPECT_EQ(perf.data[0].misses, 0);
    }

    TEST_F(ProtoDecoderTest, decodeHandlesMultipleEntries) {
        perf::PerfLog log = make_perf_log();

        auto *first = log.add_entries();
        first->set_time(0.0F);
        first->mutable_score()->set_value(10.0F);

        auto *second = log.add_entries();
        second->set_time(1.0F);
        second->mutable_score()->set_value(20.0F);

        const auto perf = decoder.decode(log);

        ASSERT_EQ(perf.data.size(), 2);
        EXPECT_FLOAT_EQ(perf.data[0].score, 10.0F);
        EXPECT_FLOAT_EQ(perf.data[1].score, 20.0F);
    }

    TEST_F(ProtoDecoderTest, decodeFileThrowsWhenFileDoesNotExist) {
        EXPECT_THROW(decoder.decode_file("this/file/does/not/exist.perf"), std::invalid_argument);
    }

    TEST_F(ProtoDecoderTest, decodeFileOfCorruptDataDoesNotThrow) {
        const auto corrupt_path = std::filesystem::path{TEST_FILES_DIR} / "corrupt.perf";
        {
            std::ofstream out(corrupt_path, std::ios::binary);
            out << "this is not a valid protobuf message";
        }

        // Documents current behavior: a failed parse is only logged to stderr,
        // decode_file still returns a (effectively empty/default) ScenarioPerf
        // rather than signaling an error to the caller.
        ksv::domain::ScenarioPerf perf;
        EXPECT_NO_THROW(perf = decoder.decode_file(corrupt_path.string()));
        EXPECT_TRUE(perf.run_id.scenario_id.name.empty());

        std::filesystem::remove(corrupt_path);
    }
}
