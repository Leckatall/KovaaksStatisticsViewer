#include <gtest/gtest.h>

#include <QSet>
#include <QString>
#include <vector>

#include "presentation/benchmark_issue_text.h"

TEST(BenchmarkIssueText, EveryCodeHasADistinctNonEmptyMessage) {
    using C = ksv::domain::BenchmarkIssueCode;
    const std::vector<C> all{
        C::MissingName, C::NoScenarios, C::NoTiers, C::DuplicateTierName,
        C::DuplicateScenarioMembership, C::DuplicateResolvedHash, C::MissingThreshold,
        C::DuplicateThreshold, C::NonFiniteThreshold, C::NegativeThreshold,
        C::NonIncreasingThreshold, C::EmptyCategory, C::EmptySubcategory,
        C::MixedCategoryContent,
    };

    QSet<QString> seen;
    for (const auto code: all) {
        const auto message = ksv::presentation::benchmarkIssueText(code);
        EXPECT_FALSE(message.isEmpty());
        seen.insert(message);
    }
    // Every code collapsing to a distinct message keeps the set size equal to the code count.
    EXPECT_EQ(seen.size(), static_cast<qsizetype>(all.size()));
}
