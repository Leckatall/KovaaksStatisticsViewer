#include <gtest/gtest.h>

#include <QString>
#include <QStringList>
#include <algorithm>
#include <string>
#include <vector>

#include "benchmarks/benchmark.h"
#include "presentation/benchmarks/manager/benchmark_group_command.h"

using namespace ksv::domain;
using namespace ksv::presentation;

namespace {
    using Kind = GroupCommandAction::Kind;

    Subcategory sub(const std::string &id, const std::string &name) {
        return Subcategory{GroupId{id}, name, BenchmarkColor{1, 2, 3, 255}, {}};
    }

    Category category(const std::string &id, const std::string &name, std::vector<Subcategory> subs = {}) {
        return Category{GroupId{id}, name, BenchmarkColor{1, 2, 3, 255}, {}, std::move(subs)};
    }

    // Clicking{Static, Dynamic}, Tracking{Smooth, Reactive}, Static, Uncategorized last.
    Benchmark library() {
        Benchmark bench;
        bench.categories = {
            category("clicking", "Clicking", {sub("c-static", "Static"), sub("c-dynamic", "Dynamic")}),
            category("tracking", "Tracking", {sub("t-smooth", "Smooth"), sub("t-reactive", "Reactive")}),
            category("static", "Static"),
        };
        return bench;
    }

    QStringList targets(const GroupCommandOptions &options) {
        QStringList ids;
        for (const auto &action: options.actions)
            ids << (action.kind == Kind::Create ? QStringLiteral("+") + action.name : action.targetId);
        return ids;
    }

    QStringList paths(const GroupCommandOptions &options) {
        QStringList result;
        for (const auto &action: options.actions)
            if (action.kind == Kind::Move) result << action.path;
        return result;
    }
}

TEST(BenchmarkGroupCommand, EmptyQueryListsEveryDestinationInHierarchyOrderWithNoCreate) {
    const auto options = buildGroupCommandOptions(library(), QString(), QString());

    EXPECT_FALSE(options.exactMatch);
    EXPECT_EQ(targets(options), (QStringList{"clicking", "c-static", "c-dynamic", "tracking", "t-smooth",
                                             "t-reactive", "static", ""}));
    EXPECT_EQ(paths(options), (QStringList{"Clicking", "Clicking › Static", "Clicking › Dynamic", "Tracking",
                                           "Tracking › Smooth", "Tracking › Reactive", "Static",
                                           "Uncategorized"}));
}

TEST(BenchmarkGroupCommand, BlankQueryIsTreatedAsEmpty) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("   "), QString());

    EXPECT_FALSE(options.exactMatch);
    EXPECT_EQ(options.actions.size(), 8U);
    EXPECT_TRUE(std::ranges::none_of(options.actions, [](const auto &a) { return a.kind == Kind::Create; }));
}

TEST(BenchmarkGroupCommand, TopLevelExactMatchIsCaseInsensitiveAndOffersNoCreate) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("  tRACKING "), QString());

    EXPECT_TRUE(options.exactMatch);
    ASSERT_FALSE(options.actions.empty());
    EXPECT_EQ(options.actions.front().kind, Kind::Move);
    EXPECT_EQ(options.actions.front().targetId, "tracking");
    EXPECT_TRUE(std::ranges::none_of(options.actions, [](const auto &a) { return a.kind == Kind::Create; }));
}

TEST(BenchmarkGroupCommand, UncategorizedIsAnExactTopLevelMatch) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("uncategorized"), QString());

    EXPECT_TRUE(options.exactMatch);
    ASSERT_EQ(options.actions.size(), 1U);
    EXPECT_EQ(options.actions.front().kind, Kind::Move);
    EXPECT_TRUE(options.actions.front().targetId.isEmpty());
    EXPECT_EQ(options.actions.front().path, "Uncategorized");
}

TEST(BenchmarkGroupCommand, UncategorizedIsNotAnExactMatchUnderAParent) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Uncategorized"), QStringLiteral("tracking"));

    EXPECT_FALSE(options.exactMatch);
    ASSERT_FALSE(options.actions.empty());
    EXPECT_EQ(options.actions.front().kind, Kind::Create);
}

TEST(BenchmarkGroupCommand, ExactMatchesComeBeforeOtherMatches) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("static"), QString());

    EXPECT_TRUE(options.exactMatch);
    EXPECT_EQ(targets(options), (QStringList{"static", "c-static"}))
        << "the top-level Static is exact; Clicking > Static only matches";
}

TEST(BenchmarkGroupCommand, StaticAtTopLevelIsNotAnExactMatchOfTheSubcategory) {
    auto bench = library();
    bench.categories.pop_back();

    const auto options = buildGroupCommandOptions(bench, QStringLiteral("Static"), QString());

    EXPECT_FALSE(options.exactMatch);
    ASSERT_EQ(options.actions.size(), 2U);
    EXPECT_EQ(options.actions.at(0).kind, Kind::Create);
    EXPECT_EQ(options.actions.at(0).name, "Static");
    EXPECT_TRUE(options.actions.at(0).parentId.isEmpty());
    EXPECT_EQ(options.actions.at(1).targetId, "c-static");
}

TEST(BenchmarkGroupCommand, UnderAParentOnlyThatParentsSubcategoryIsExact) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Static"), QStringLiteral("clicking"));

    EXPECT_TRUE(options.exactMatch);
    EXPECT_EQ(targets(options), (QStringList{"c-static", "static"}))
        << "the top-level Static only matches, because the parent is Clicking";
}

TEST(BenchmarkGroupCommand, ASubcategoryOfAnotherParentIsNotExact) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Static"), QStringLiteral("tracking"));

    EXPECT_FALSE(options.exactMatch);
    ASSERT_FALSE(options.actions.empty());
    EXPECT_EQ(options.actions.front().kind, Kind::Create);
    EXPECT_EQ(options.actions.front().parentId, "tracking");
}

TEST(BenchmarkGroupCommand, NoExactMatchPutsCreateFirstThenMatches) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Tracking drills"), QString());

    EXPECT_FALSE(options.exactMatch);
    ASSERT_GE(options.actions.size(), 1U);
    EXPECT_EQ(options.actions.front().kind, Kind::Create);
    EXPECT_EQ(options.actions.front().name, "Tracking drills");
    EXPECT_EQ(targets(options).mid(1), (QStringList{"tracking", "t-smooth", "t-reactive"}))
        << "the first word still suggests Tracking and everything beneath it";
}

TEST(BenchmarkGroupCommand, AMatchNeedOnlyContainTheQueryInsideItsPath) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("eact"), QString());

    EXPECT_EQ(targets(options), (QStringList{"+eact", "t-reactive"}));
}

TEST(BenchmarkGroupCommand, ANonMatchingQueryOffersOnlyCreate) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Zzz"), QString());

    EXPECT_EQ(targets(options), QStringList{"+Zzz"});
}

TEST(BenchmarkGroupCommand, RepeatedNamesStaySeparateActionsWithTheirOwnIds) {
    auto bench = library();
    bench.categories.push_back(category("tracking-2", "Tracking"));
    bench.categories.front().subcategories.push_back(sub("c-static-2", "Static"));

    const auto tracking = buildGroupCommandOptions(bench, QStringLiteral("Tracking"), QString());
    EXPECT_TRUE(tracking.exactMatch);
    EXPECT_EQ(targets(tracking).mid(0, 2), (QStringList{"tracking", "tracking-2"}));

    const auto inClicking = buildGroupCommandOptions(bench, QStringLiteral("Static"), QStringLiteral("clicking"));
    EXPECT_TRUE(inClicking.exactMatch);
    EXPECT_EQ(targets(inClicking).mid(0, 2), (QStringList{"c-static", "c-static-2"}));
}

TEST(BenchmarkGroupCommand, ResultsAreNotCapped) {
    Benchmark bench;
    for (int i = 0; i < 12; ++i)
        bench.categories.push_back(category("c" + std::to_string(i), "Drill " + std::to_string(i)));

    const auto options = buildGroupCommandOptions(bench, QStringLiteral("drill"), QString());

    EXPECT_EQ(options.actions.size(), 13U) << "create plus all twelve categories";
}

TEST(BenchmarkGroupCommand, MatchingNeverFiltersByDestinationLevel) {
    const auto options = buildGroupCommandOptions(library(), QStringLiteral("Smooth"), QStringLiteral("clicking"));

    EXPECT_FALSE(options.exactMatch);
    EXPECT_EQ(targets(options), (QStringList{"+Smooth", "t-smooth"}));
}
