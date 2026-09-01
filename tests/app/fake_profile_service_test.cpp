//
// Pins the parts of the shared FakeProfileService that carry behaviour of their
// own, rather than returning a field verbatim.
//

#include <gtest/gtest.h>

#include "fake_profile_service.h"
#include "run_builders.h"

using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    TEST(FakeProfileServiceTest, GetMostRecentRunsReturnsTheNewestUpToTheRequestedCount) {
        FakeProfileService profile;
        const ScenarioId scenario{.name = "Scenario hash-1", .hash = "hash-1"};
        profile.perfs_by_scenario[scenario] = {
            makeRun("hash-1", 100), makeRun("hash-1", 200), makeRun("hash-1", 300)
        };

        const auto recent = profile.getMostRecentRuns(scenario, 2);

        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 200);
        EXPECT_EQ(recent[1].run_id.start_time, 300);
    }
}
