#include <chrono>

#include <gtest/gtest.h>

#include "formats/csv/run_filename.h"

namespace {
    using namespace std::chrono_literals;

    TEST(RunFilenameTest, NormalizesProductionAndBareFixtureStems) {
        EXPECT_EQ(ksv::data::pairingStem("Scenario - Challenge - 2026.08.27-02.26.40 Performance.perf"),
                  "Scenario - Challenge - 2026.08.27-02.26.40");
        EXPECT_EQ(ksv::data::pairingStem("Scenario - Challenge - 2026.08.27-02.26.40 Stats.csv"),
                  "Scenario - Challenge - 2026.08.27-02.26.40");
        EXPECT_EQ(ksv::data::pairingStem("1wall6targets TE.perf"), "1wall6targets TE");
        EXPECT_EQ(ksv::data::pairingStem("1wall6targets TE.csv"), "1wall6targets TE");
    }

    TEST(RunFilenameTest, DerivesStatsSibling) {
        const ksv::application::PerfFile perf{
            .root = "C:/Kovaaks",
            .subdir = "FPSAimTrainer/performances",
            .filename = "Scenario - Challenge - 2026.08.27-02.26.40 Performance.perf",
        };

        const auto csv = ksv::data::statsSibling(perf);

        EXPECT_EQ(csv.root, perf.root);
        EXPECT_EQ(csv.subdir, "FPSAimTrainer/stats");
        EXPECT_EQ(csv.filename, "Scenario - Challenge - 2026.08.27-02.26.40 Stats.csv");
    }

    TEST(RunFilenameTest, DerivesCsvOnlyStartAndLength) {
        const auto timing = ksv::data::deriveCsvTiming(
            "Scenario - Challenge - 2026.08.27-02.26.40 Stats.csv", 2h + 25min + 40s + 148ms);

        ASSERT_TRUE(timing);
        EXPECT_NEAR(timing->scenario_length, 59.852F, 0.001F);
        EXPECT_FALSE(timing->crossed_midnight);
    }

    TEST(RunFilenameTest, RollsChallengeStartBackAcrossMidnight) {
        const auto timing = ksv::data::deriveCsvTiming(
            "Scenario - Challenge - 2026.08.27-00.00.10 Stats.csv", 23h + 59min + 50s);

        ASSERT_TRUE(timing);
        EXPECT_NEAR(timing->scenario_length, 20.0F, 0.001F);
        EXPECT_TRUE(timing->crossed_midnight);
    }
}
