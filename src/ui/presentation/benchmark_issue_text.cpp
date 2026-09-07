#include "benchmark_issue_text.h"

#include <QCoreApplication>

namespace ksv::presentation {
    QString benchmarkIssueText(domain::BenchmarkIssueCode code) {
        // A default-free switch: a future BenchmarkIssueCode breaks the build here instead of
        // silently rendering an empty message.
        switch (code) {
            case domain::BenchmarkIssueCode::MissingName:
                return QCoreApplication::translate("BenchmarkIssue", "The benchmark has no name.");
            case domain::BenchmarkIssueCode::NoScenarios:
                return QCoreApplication::translate("BenchmarkIssue", "Add at least one scenario.");
            case domain::BenchmarkIssueCode::NoTiers:
                return QCoreApplication::translate("BenchmarkIssue", "Add at least one tier.");
            case domain::BenchmarkIssueCode::DuplicateTierName:
                return QCoreApplication::translate("BenchmarkIssue", "Two tiers share the same name.");
            case domain::BenchmarkIssueCode::DuplicateScenarioMembership:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "The same scenario appears more than once.");
            case domain::BenchmarkIssueCode::DuplicateResolvedHash:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "Two scenarios map to the same profile scenario.");
            case domain::BenchmarkIssueCode::MissingThreshold:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "A scenario is missing a threshold for some tier.");
            case domain::BenchmarkIssueCode::DuplicateThreshold:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "A scenario has two thresholds for one tier.");
            case domain::BenchmarkIssueCode::NonFiniteThreshold:
                return QCoreApplication::translate("BenchmarkIssue", "A threshold score is not a finite number.");
            case domain::BenchmarkIssueCode::NegativeThreshold:
                return QCoreApplication::translate("BenchmarkIssue", "A threshold score is negative.");
            case domain::BenchmarkIssueCode::NonIncreasingThreshold:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "Thresholds must strictly increase in tier order.");
            case domain::BenchmarkIssueCode::EmptyCategory:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "A category has neither scenarios nor subcategories.");
            case domain::BenchmarkIssueCode::EmptySubcategory:
                return QCoreApplication::translate("BenchmarkIssue", "A subcategory has no scenarios.");
            case domain::BenchmarkIssueCode::MixedCategoryContent:
                return QCoreApplication::translate("BenchmarkIssue",
                                                    "A category holds both direct scenarios and subcategories.");
        }
        return {};
    }
}
