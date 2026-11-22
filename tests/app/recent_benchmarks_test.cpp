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
    using Files = std::vector<std::string>;

    struct Fixture {
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::unique_ptr<RecentBenchmarks> recent;
        int changes = 0;

        void build(const Files &stored = {}) {
            settings->recentBenchmarkFiles = stored;
            recent = std::make_unique<RecentBenchmarks>(settings);
            recent->onChanged([this] { ++changes; });
        }
    };
}

TEST(RecentBenchmarks, LoadsTheStoredListOnConstruction) {
    Fixture fixture;
    fixture.build({"b", "a"});
    EXPECT_EQ(fixture.recent->filenames(), (Files{"b", "a"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkFilesWrites, 0);
}

TEST(RecentBenchmarks, MarkViewedInsertsANewFilenameAtTheFront) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markViewed("c");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"c", "b", "a"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkViewedMovesAnExistingFilenameToTheFront) {
    Fixture fixture;
    fixture.build({"c", "b", "a"});

    fixture.recent->markViewed("a");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"a", "c", "b"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkViewedOnTheFrontFilenameChangesNothing) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markViewed("b");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"b", "a"}));
    EXPECT_EQ(fixture.changes, 0);
    EXPECT_EQ(fixture.settings->recentBenchmarkFilesWrites, 0);
}

TEST(RecentBenchmarks, MarkCreatedOnAnEmptyListTakesTheFront) {
    Fixture fixture;
    fixture.build();

    fixture.recent->markCreated("new");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"new"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkCreatedSitsBehindTheMostRecentlyViewed) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markCreated("new");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"b", "new", "a"}));
    EXPECT_EQ(fixture.changes, 1);
}

TEST(RecentBenchmarks, MarkCreatedOnAFilenameAlreadyPresentChangesNothing) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->markCreated("a");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"b", "a"}));
    EXPECT_EQ(fixture.changes, 0);
    EXPECT_EQ(fixture.settings->recentBenchmarkFilesWrites, 0);
}

TEST(RecentBenchmarks, EveryChangeIsWrittenThroughAndSurvivesIntoANewInstance) {
    Fixture fixture;
    fixture.build();

    fixture.recent->markViewed("a");
    fixture.recent->markViewed("b");
    fixture.recent->markCreated("c");

    EXPECT_EQ(fixture.settings->recentBenchmarkFiles, (Files{"b", "c", "a"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkFilesWrites, 3);

    const RecentBenchmarks reloaded(fixture.settings);
    EXPECT_EQ(reloaded.filenames(), (Files{"b", "c", "a"}));
}

TEST(RecentBenchmarks, RenameKeepsThePositionAtTheFrontInTheMiddleAndLast) {
    for (const auto &[from, to, expected]: {std::tuple{"c", "C", Files{"C", "b", "a"}},
                                           std::tuple{"b", "B", Files{"c", "B", "a"}},
                                           std::tuple{"a", "A", Files{"c", "b", "A"}}}) {
        Fixture fixture;
        fixture.build({"c", "b", "a"});

        fixture.recent->rename(from, to);

        EXPECT_EQ(fixture.recent->filenames(), expected) << from;
        EXPECT_EQ(fixture.settings->recentBenchmarkFiles, expected) << from;
        EXPECT_EQ(fixture.changes, 1) << from;
    }
}

TEST(RecentBenchmarks, RenamingAnAbsentFilenameChangesNothing) {
    Fixture fixture;
    fixture.build({"b", "a"});

    fixture.recent->rename("x", "y");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"b", "a"}));
    EXPECT_EQ(fixture.changes, 0);
    EXPECT_EQ(fixture.settings->recentBenchmarkFilesWrites, 0);
}

TEST(RecentBenchmarks, RenamingOntoAFilenameAlreadyPresentLeavesOneOccurrence) {
    Fixture fixture;
    fixture.build({"c", "b", "a"});

    fixture.recent->rename("b", "a");

    EXPECT_EQ(fixture.recent->filenames(), (Files{"c", "a"}));
    EXPECT_EQ(fixture.changes, 1);
}
