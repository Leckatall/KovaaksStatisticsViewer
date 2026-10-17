#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "fake_settings_service.h"
#include "usecases/recent_benchmarks.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    using Ids = std::vector<std::string>;

    struct Fixture {
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::unique_ptr<RecentBenchmarks> recent;
        int changes = 0;

        void build(const Ids &stored = {}) {
            settings->recentBenchmarkIds = stored;
            recent = std::make_unique<RecentBenchmarks>(settings);
            recent->onChanged([this] { ++changes; });
        }
    };
}

TEST(RecentBenchmarks, LoadsTheStoredListOnConstruction) {
    Fixture fixture;
    fixture.build({"b", "a"});
    EXPECT_EQ(fixture.recent->ids(), (Ids{"b", "a"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, 0);
}

TEST(RecentBenchmarks, MarkViewedInsertsANewIdAtTheFront) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markViewed("c");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"c", "b", "a"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkViewedMovesAnExistingIdToTheFront) {
    Fixture fixture;
    fixture.build({"c", "b", "a"});

    fixture.recent->markViewed("a");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"a", "c", "b"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkViewedOnTheFrontIdChangesNothing) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markViewed("b");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"b", "a"}));
    EXPECT_EQ(fixture.changes, 0);
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, 0);
}

TEST(RecentBenchmarks, MarkCreatedOnAnEmptyListTakesTheFront) {
    Fixture fixture;
    fixture.build();

    fixture.recent->markCreated("new");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"new"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkCreatedSitsBehindTheMostRecentlyViewed) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markCreated("new");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"b", "new", "a"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkCreatedOnAnIdAlreadyPresentChangesNothing) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markCreated("a");

    EXPECT_EQ(fixture.recent->ids(), (Ids{"b", "a"}));
    EXPECT_EQ(fixture.changes, 0);
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, 0);
}

TEST(RecentBenchmarks, EveryChangeIsWrittenThroughAndSurvivesIntoANewInstance) {
    Fixture fixture;
    fixture.build();

    fixture.recent->markViewed("a");
    fixture.recent->markViewed("b");
    fixture.recent->markCreated("c");

    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b", "c", "a"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, 3);

    const RecentBenchmarks reloaded(fixture.settings);
    EXPECT_EQ(reloaded.ids(), (Ids{"b", "c", "a"}));
}
