//
// ProfileBuilder tests using a hand-written fake IFileService.
//

#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <unordered_map>

#include "profile_builder.h"
#include "fake_file_service.h"

using namespace ksv::data;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    ksv::domain::ScenarioPerf make_perf(const std::string &hash, const long long start_time,
                                        const float score = 0.0F) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.add_data(0.0F, ksv::domain::SCORE, score);
        return perf;
    }

    class ProfileBuilderTest : public testing::Test {
    protected:
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        ProfileBuilder builder{fake_file_service};
    };

    TEST_F(ProfileBuilderTest, BuildRegistersEveryConfiguredRoot) {
        fake_file_service->source_roots = {"C:/Kovaaks", "D:/Kovaaks"};

        const auto profile = builder.build();
        const auto &entries = profile.sources().entries();
        ASSERT_EQ(entries.size(), 4);
        EXPECT_EQ(entries[0].path, "C:/Kovaaks");
        EXPECT_EQ(entries[2].path, "D:/Kovaaks");
    }

    TEST_F(ProfileBuilderTest, BuildAggregatesEveryPerfFromTheDirectory) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-1", 200), make_perf("hash-2", 300)
        };

        const auto profile = builder.build();

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 2);
        EXPECT_EQ(profile.getRunCount(scenarios[0]).value_or(0) +
                  profile.getRunCount(scenarios[1]).value_or(0), 3);
        for (const auto &run : profile.getAllRunRecords()) {
            EXPECT_TRUE(profile.sources().resolve(run.source).has_value());
        }
    }

    TEST_F(ProfileBuilderTest, BuildReportsProgressOncePerFile) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 200), make_perf("hash-3", 300)
        };

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        std::ignore = builder.build([&reports](const std::size_t done, const std::size_t total) {
            reports.emplace_back(done, total);
        });

        ASSERT_EQ(reports.size(), 3);
        EXPECT_EQ(reports[0], (std::pair<std::size_t, std::size_t>{1, 3}));
        EXPECT_EQ(reports[1], (std::pair<std::size_t, std::size_t>{2, 3}));
        EXPECT_EQ(reports[2], (std::pair<std::size_t, std::size_t>{3, 3}));
    }

    TEST_F(ProfileBuilderTest, BuildSkipsAFileThatVanishedDuringDecodeButAggregatesTheRest) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 200), make_perf("hash-3", 300)
        };
        fake_file_service->throw_for_indices.insert(1);

        std::vector<std::pair<std::size_t, std::size_t>> reports;
        const auto profile = builder.build([&reports](const std::size_t done, const std::size_t total) {
            reports.emplace_back(done, total);
        });

        EXPECT_EQ(profile.getAllRunRecords().size(), 2);
        ASSERT_FALSE(reports.empty());
        EXPECT_EQ(reports.back(), (std::pair<std::size_t, std::size_t>{3, 3}));
    }

    TEST_F(ProfileBuilderTest, BuildReportsNoProgressForAnEmptyDirectory) {
        int report_count = 0;
        std::ignore = builder.build([&report_count](std::size_t, std::size_t) { ++report_count; });

        EXPECT_EQ(report_count, 0);
    }

    TEST_F(ProfileBuilderTest, BuildIsRepeatable) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        const auto first = builder.build();
        const auto second = builder.build();

        EXPECT_EQ(first.getScenarioList().size(), 1);

        EXPECT_EQ(second.getScenarioList().size(), 1);
    }
}
