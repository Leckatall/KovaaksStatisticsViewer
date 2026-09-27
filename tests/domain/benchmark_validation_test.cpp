#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "benchmarks/benchmark_validation.h"

using namespace ksv::domain;

namespace {
    // Minimal helper builders keep each test focused on the rule under test.
    Tier tier(std::string id, std::string name, uint8_t r = 0) {
        return Tier{TierId{std::move(id)}, std::move(name), BenchmarkColor{r, 0, 0, 255}};
    }

    ScenarioEntry entry(std::string id, std::string name, std::vector<Threshold> thresholds,
                        std::optional<std::string> hash = std::nullopt) {
        return ScenarioEntry{ScenarioEntryId{std::move(id)}, std::move(name), std::move(hash),
                             std::move(thresholds)};
    }

    // A benchmark that is Trackable: one tier, one uncategorized scenario with its threshold.
    Benchmark trackable() {
        Benchmark def;
        def.id = BenchmarkId{"b1"};
        def.name = "Bench";
        def.tiers = {tier("t1", "Bronze")};
        def.uncategorized = {entry("s1", "Scenario One", {Threshold{TierId{"t1"}, 100.0}})};
        return def;
    }

    bool hasCode(const CompletenessResult &result, BenchmarkIssueCode code) {
        return std::ranges::any_of(result.issues, [&](const auto &i) { return i.code == code; });
    }
}

TEST(BenchmarkValidation, MinimalCompleteBenchmarkIsTrackableWithNoIssues) {
    const auto result = validateBenchmark(trackable());
    EXPECT_EQ(result.completeness, Completeness::Trackable);
    EXPECT_TRUE(result.issues.empty());
}

TEST(BenchmarkValidation, EmptyNameIsIncomplete) {
    auto def = trackable();
    def.name.clear();
    const auto result = validateBenchmark(def);
    EXPECT_EQ(result.completeness, Completeness::Incomplete);
    EXPECT_TRUE(hasCode(result, BenchmarkIssueCode::MissingName));
}

TEST(BenchmarkValidation, NoTiersIsIncomplete) {
    auto def = trackable();
    def.tiers.clear();
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::NoTiers));
}

TEST(BenchmarkValidation, NoScenariosIsIncomplete) {
    auto def = trackable();
    def.uncategorized.clear();
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::NoScenarios));
}

TEST(BenchmarkValidation, DuplicateTierNameIsIncomplete) {
    auto def = trackable();
    def.tiers.push_back(tier("t2", "Bronze"));  // same display name
    def.uncategorized[0].thresholds.push_back(Threshold{TierId{"t2"}, 200.0});
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::DuplicateTierName));
}

TEST(BenchmarkValidation, MissingThresholdForATierIsIncomplete) {
    auto def = trackable();
    def.tiers.push_back(tier("t2", "Silver"));  // no threshold added for t2
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::MissingThreshold));
}

TEST(BenchmarkValidation, DuplicateThresholdForATierIsIncomplete) {
    auto def = trackable();
    def.uncategorized[0].thresholds.push_back(Threshold{TierId{"t1"}, 150.0});
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::DuplicateThreshold));
}

TEST(BenchmarkValidation, NonIncreasingThresholdsAreIncomplete) {
    auto def = trackable();
    def.tiers.push_back(tier("t2", "Silver"));
    def.uncategorized[0].thresholds.push_back(Threshold{TierId{"t2"}, 100.0});  // equal to t1
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::NonIncreasingThreshold));
}

TEST(BenchmarkValidation, NegativeAndNonFiniteThresholdsAreIncomplete) {
    auto negative = trackable();
    negative.uncategorized[0].thresholds[0].score = -1.0;
    EXPECT_TRUE(hasCode(validateBenchmark(negative), BenchmarkIssueCode::NegativeThreshold));

    auto nan = trackable();
    nan.uncategorized[0].thresholds[0].score = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(hasCode(validateBenchmark(nan), BenchmarkIssueCode::NonFiniteThreshold));
}

TEST(BenchmarkValidation, EmptyUserCategoryIsIncompleteButEmptyUncategorizedIsNot) {
    auto def = trackable();
    def.categories.push_back(Category{GroupId{"g1"}, "Cat", {}, {}, {}});
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::EmptyCategory));

    auto onlyEmptyUncategorized = trackable();  // uncategorized already the sole scenario holder
    EXPECT_FALSE(hasCode(validateBenchmark(onlyEmptyUncategorized), BenchmarkIssueCode::EmptyCategory));
}

TEST(BenchmarkValidation, MixedCategoryContentIsIncomplete) {
    auto def = trackable();
    Category mixed{GroupId{"g1"}, "Cat", {}, {entry("s2", "Two", {Threshold{TierId{"t1"}, 10.0}})},
                   {Subcategory{GroupId{"g2"}, "Sub", {},
                                {entry("s3", "Three", {Threshold{TierId{"t1"}, 10.0}})}}}};
    def.categories.push_back(std::move(mixed));
    EXPECT_TRUE(hasCode(validateBenchmark(def), BenchmarkIssueCode::MixedCategoryContent));
}

TEST(BenchmarkValidation, DuplicateMembershipByNameAndByHashAreIncomplete) {
    auto byName = trackable();
    byName.uncategorized.push_back(entry("s2", "Scenario One", {Threshold{TierId{"t1"}, 50.0}}));
    EXPECT_TRUE(hasCode(validateBenchmark(byName), BenchmarkIssueCode::DuplicateScenarioMembership));

    auto byHash = trackable();
    byHash.uncategorized[0].hash = "HASH";
    byHash.uncategorized.push_back(
        entry("s2", "Other", {Threshold{TierId{"t1"}, 50.0}}, std::string{"HASH"}));
    EXPECT_TRUE(hasCode(validateBenchmark(byHash), BenchmarkIssueCode::DuplicateResolvedHash));
}

TEST(BenchmarkValidation, UnresolvedScenarioDoesNotBlockTrackable) {
    auto def = trackable();
    def.uncategorized[0].hash = std::nullopt;  // unresolved: still structurally complete
    EXPECT_EQ(validateBenchmark(def).completeness, Completeness::Trackable);
}

namespace {
    std::vector<BenchmarkIssue> issuesWith(const CompletenessResult &result, BenchmarkIssueCode code) {
        std::vector<BenchmarkIssue> found;
        for (const auto &issue: result.issues)
            if (issue.code == code) found.push_back(issue);
        return found;
    }

    std::vector<std::string> tierIdsOf(const std::vector<BenchmarkIssue> &issues) {
        std::vector<std::string> ids;
        for (const auto &issue: issues) ids.push_back(issue.tierId ? issue.tierId->value : "<none>");
        return ids;
    }
}

TEST(BenchmarkTableValidation, MissingElementNames) {
    Benchmark def;
    def.id = BenchmarkId{"b1"};
    def.name = "Bench";
    def.tiers = {tier("t1", " \t"), tier("t2", "Silver")};
    const std::vector<Threshold> scores{{TierId{"t1"}, 1.0}, {TierId{"t2"}, 2.0}};
    def.uncategorized = {entry("e1", "", scores), entry("e3", "Named", scores)};
    Category category{GroupId{"c1"}, "", {}, {}, {}};
    category.subcategories.push_back(Subcategory{GroupId{"s1"}, "  ", {}, {entry("e2", "Other", scores)}});
    def.categories = {category};

    const auto result = validateBenchmark(def);

    EXPECT_EQ(result.completeness, Completeness::Incomplete);
    const auto tierNames = issuesWith(result, BenchmarkIssueCode::MissingTierName);
    ASSERT_EQ(tierNames.size(), 1U);
    EXPECT_EQ(tierNames.front().target, IssueTarget{TierId{"t1"}});
    const auto scenarioNames = issuesWith(result, BenchmarkIssueCode::MissingScenarioName);
    ASSERT_EQ(scenarioNames.size(), 1U);
    EXPECT_EQ(scenarioNames.front().target, IssueTarget{ScenarioEntryId{"e1"}});
    // Uncategorized is neutral and has no user name to be missing; only c1 and s1 qualify.
    const auto groupNames = issuesWith(result, BenchmarkIssueCode::MissingGroupName);
    ASSERT_EQ(groupNames.size(), 2U);
    EXPECT_EQ(groupNames.at(0).target, IssueTarget{GroupId{"c1"}});
    EXPECT_EQ(groupNames.at(1).target, IssueTarget{GroupId{"s1"}});
    EXPECT_FALSE(hasCode(result, BenchmarkIssueCode::MissingName));
}

TEST(BenchmarkTableValidation, ThresholdCellLocations) {
    Benchmark def;
    def.id = BenchmarkId{"b1"};
    def.name = "Bench";
    def.tiers = {tier("t1", "A"), tier("t2", "B"), tier("t3", "C"), tier("t4", "D")};
    def.uncategorized = {entry("e", "Scenario", {{TierId{"t2"}, -1.0}, {TierId{"t3"}, 10.0},
                                                 {TierId{"t3"}, 11.0}, {TierId{"t4"}, 5.0}})};

    const auto result = validateBenchmark(def);

    const auto expectCell = [&](BenchmarkIssueCode code, const std::vector<std::string> &tiers) {
        const auto issues = issuesWith(result, code);
        for (const auto &issue: issues) EXPECT_EQ(issue.target, IssueTarget{ScenarioEntryId{"e"}});
        EXPECT_EQ(tierIdsOf(issues), tiers) << static_cast<int>(code);
    };
    expectCell(BenchmarkIssueCode::MissingThreshold, {"t1"});
    expectCell(BenchmarkIssueCode::NegativeThreshold, {"t2"});
    expectCell(BenchmarkIssueCode::DuplicateThreshold, {"t3"});
    expectCell(BenchmarkIssueCode::NonIncreasingThreshold, {"t4"});

    // The offending tier follows the current ladder order, not the order thresholds were stored.
    def.tiers = {tier("t4", "D"), tier("t3", "C"), tier("t2", "B"), tier("t1", "A")};
    EXPECT_EQ(tierIdsOf(issuesWith(validateBenchmark(def), BenchmarkIssueCode::NonIncreasingThreshold)),
              std::vector<std::string>{"t2"});

    Benchmark legacy = trackable();
    legacy.name.clear();
    legacy.uncategorized.front().thresholds.front().score = std::numeric_limits<double>::quiet_NaN();
    legacy.categories.push_back(Category{GroupId{"empty"}, "Empty", {}, {}, {}});
    const auto legacyResult = validateBenchmark(legacy);
    EXPECT_EQ(tierIdsOf(issuesWith(legacyResult, BenchmarkIssueCode::NonFiniteThreshold)),
              std::vector<std::string>{"t1"});
    EXPECT_EQ(tierIdsOf(issuesWith(legacyResult, BenchmarkIssueCode::MissingName)),
              std::vector<std::string>{"<none>"});
    EXPECT_EQ(tierIdsOf(issuesWith(legacyResult, BenchmarkIssueCode::EmptyCategory)),
              std::vector<std::string>{"<none>"});
}
