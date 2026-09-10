// BenchmarkBreakdownModel is a read-only QAbstractItemModel that BenchmarkTrackingViewModel owns.
// It reconstructs the Uncategorized/category/subcategory/scenario tree from the selected
// definition joined with its projection, keys nodes by stable benchmark element IDs, and owns the
// expansion state that BenchmarkTrackingViewModel::setExpanded drives.

#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
#include <QSignalSpy>
#include <memory>

#include "presentation/benchmark_tracking_vm.h"
#include "presentation/benchmark_breakdown_model.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using ksv::tests_ui::FakeBenchmarkTrackingUseCase;
using ksv::tests_ui::makeBreakdownSnapshot;

namespace {
    int roleFor(const QAbstractItemModel *model, const char *name) {
        const auto names = model->roleNames();
        for (auto it = names.begin(); it != names.end(); ++it)
            if (it.value() == name) return it.key();
        return -1;
    }

    QString roleText(const QAbstractItemModel *model, const QModelIndex &index, const char *role) {
        return model->data(index, roleFor(model, role)).toString();
    }

    std::shared_ptr<FakeBenchmarkTrackingUseCase> breakdownFake() {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->snap = makeBreakdownSnapshot();
        return fake;
    }
}

TEST(BenchmarkBreakdownModel, ReconstructsHierarchyShapeAndOrder) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();
    ASSERT_NE(model, nullptr);

    // Uncategorized first, then categories in definition order.
    ASSERT_EQ(model->rowCount(QModelIndex()), 3);
    EXPECT_EQ(roleText(model, model->index(0, 0), "kind"), "uncategorized");
    EXPECT_EQ(roleText(model, model->index(1, 0), "kind"), "category");
    EXPECT_EQ(roleText(model, model->index(1, 0), "name"), "Clicking");
    EXPECT_EQ(roleText(model, model->index(2, 0), "name"), "Tracking");

    const QModelIndex uncategorized = model->index(0, 0);
    ASSERT_EQ(model->rowCount(uncategorized), 2);
    EXPECT_EQ(roleText(model, model->index(0, 0, uncategorized), "kind"), "scenario");

    EXPECT_EQ(model->rowCount(model->index(1, 0)), 2);

    const QModelIndex tracking = model->index(2, 0);
    ASSERT_EQ(model->rowCount(tracking), 1);
    const QModelIndex smoothness = model->index(0, 0, tracking);
    EXPECT_EQ(roleText(model, smoothness, "kind"), "subcategory");
    EXPECT_EQ(roleText(model, smoothness, "name"), "Smoothness");
    ASSERT_EQ(model->rowCount(smoothness), 1);
    EXPECT_EQ(roleText(model, model->index(0, 0, smoothness), "kind"), "scenario");
}

TEST(BenchmarkBreakdownModel, StableElementIdsAreNodeKeys) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    EXPECT_EQ(roleText(model, model->index(1, 0), "nodeId"), "c1");
    EXPECT_EQ(roleText(model, model->index(0, 0, model->index(1, 0)), "nodeId"), "e1");
    const QModelIndex smoothness = model->index(0, 0, model->index(2, 0));
    EXPECT_EQ(roleText(model, smoothness, "nodeId"), "sc1");
    EXPECT_EQ(roleText(model, model->index(0, 0, smoothness), "nodeId"), "e3");
}

TEST(BenchmarkBreakdownModel, ExposesEveryRequiredRole) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    const QAbstractItemModel *model = vm.breakdown();

    for (const char *role : {"nodeId", "kind", "name", "color", "highestTierText", "childCount",
                             "nextTierBlocker", "personalBestText", "recentAverageText",
                             "recentSampleCount", "attainedTierText", "nextThresholdText",
                             "matchStateText", "expanded"})
        EXPECT_NE(roleFor(model, role), -1) << role;
}

TEST(BenchmarkBreakdownModel, ScenarioTextualFallbacks) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    // "e2" under Clicking: no runs, no attainment, no next threshold.
    const QModelIndex e2 = model->index(1, 0, model->index(1, 0));
    EXPECT_EQ(roleText(model, e2, "personalBestText"), "Unplayed");
    EXPECT_EQ(roleText(model, e2, "recentAverageText"), "No completed runs");
    EXPECT_EQ(roleText(model, e2, "attainedTierText"), "Unranked");
    EXPECT_EQ(roleText(model, e2, "nextThresholdText"), "Highest tier attained");
}

TEST(BenchmarkBreakdownModel, ScenarioPopulatedValues) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    const QModelIndex e1 = model->index(0, 0, model->index(1, 0));
    EXPECT_TRUE(roleText(model, e1, "personalBestText").contains("800"));
    EXPECT_TRUE(roleText(model, e1, "recentAverageText").contains("780"));
    EXPECT_EQ(roleText(model, e1, "attainedTierText"), "Bronze");
    EXPECT_TRUE(roleText(model, e1, "nextThresholdText").contains("850"));
    EXPECT_EQ(model->data(e1, roleFor(model, "recentSampleCount")).toInt(), 3);
}

TEST(BenchmarkBreakdownModel, RecentSampleCountOnlyReportedBetweenOneAndFour) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();
    const int role = roleFor(model, "recentSampleCount");

    const QModelIndex e2 = model->index(1, 0, model->index(1, 0)); // sample count 0
    const QModelIndex e3 = model->index(0, 0, model->index(0, 0, model->index(2, 0))); // sample count 5
    EXPECT_EQ(model->data(e2, role).toInt(), 0);
    EXPECT_EQ(model->data(e3, role).toInt(), 0);
}

TEST(BenchmarkBreakdownModel, MatchStateLabels) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    const QModelIndex u1 = model->index(0, 0, model->index(0, 0));
    const QModelIndex u2 = model->index(1, 0, model->index(0, 0));
    const QModelIndex e1 = model->index(0, 0, model->index(1, 0));
    const QModelIndex e2 = model->index(1, 0, model->index(1, 0));
    const QModelIndex e3 = model->index(0, 0, model->index(0, 0, model->index(2, 0)));

    EXPECT_EQ(roleText(model, e1, "matchStateText"), "Resolved");
    EXPECT_EQ(roleText(model, e2, "matchStateText"), "Mapped, unavailable");
    EXPECT_EQ(roleText(model, u1, "matchStateText"), "Unresolved");
    EXPECT_EQ(roleText(model, e3, "matchStateText"), "Ambiguous");
    EXPECT_EQ(roleText(model, u2, "matchStateText"), "Pending automatic mapping");
}

TEST(BenchmarkBreakdownModel, UncategorizedIsNeutralWhileGroupsCarryColorAndName) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();
    const int colorRole = roleFor(model, "color");

    EXPECT_FALSE(model->data(model->index(0, 0), colorRole).value<QColor>().isValid());

    const QModelIndex clicking = model->index(1, 0);
    EXPECT_EQ(model->data(clicking, colorRole).value<QColor>(), QColor(255, 0, 0));
    EXPECT_EQ(roleText(model, clicking, "name"), "Clicking");

    const QModelIndex smoothness = model->index(0, 0, model->index(2, 0));
    EXPECT_EQ(model->data(smoothness, colorRole).value<QColor>(), QColor(0, 200, 0));
    EXPECT_EQ(roleText(model, smoothness, "name"), "Smoothness");
}

TEST(BenchmarkBreakdownModel, GroupHighestSatisfiedTierAndChildCount) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    const QModelIndex clicking = model->index(1, 0);
    EXPECT_EQ(roleText(model, clicking, "highestTierText"), "Bronze");
    EXPECT_EQ(model->data(clicking, roleFor(model, "childCount")).toInt(), 2);

    const QModelIndex smoothness = model->index(0, 0, model->index(2, 0));
    EXPECT_EQ(roleText(model, smoothness, "highestTierText"), "Silver");
    EXPECT_EQ(model->data(smoothness, roleFor(model, "childCount")).toInt(), 1);
}

TEST(BenchmarkBreakdownModel, BlockerFlagPropagatesToNamedGroupAndScenarioNodes) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();
    const int blocker = roleFor(model, "nextTierBlocker");

    const QModelIndex clicking = model->index(1, 0);
    const QModelIndex e1 = model->index(0, 0, clicking);
    const QModelIndex e2 = model->index(1, 0, clicking);
    const QModelIndex smoothness = model->index(0, 0, model->index(2, 0));
    const QModelIndex e3 = model->index(0, 0, smoothness);
    const QModelIndex u1 = model->index(0, 0, model->index(0, 0));

    EXPECT_TRUE(model->data(e2, blocker).toBool());
    EXPECT_TRUE(model->data(clicking, blocker).toBool());
    EXPECT_TRUE(model->data(e3, blocker).toBool());
    EXPECT_TRUE(model->data(smoothness, blocker).toBool());
    EXPECT_FALSE(model->data(e1, blocker).toBool());
    EXPECT_FALSE(model->data(u1, blocker).toBool());
}

TEST(BenchmarkBreakdownModel, TopLevelGroupsAndSubcategoriesStartExpandedAndSetExpandedIsReadable) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();
    const int expanded = roleFor(model, "expanded");

    const QModelIndex clicking = model->index(1, 0);
    const QModelIndex smoothness = model->index(0, 0, model->index(2, 0));
    EXPECT_TRUE(model->data(clicking, expanded).toBool());
    EXPECT_TRUE(model->data(smoothness, expanded).toBool());

    const QSignalSpy spy(model, &QAbstractItemModel::dataChanged);
    vm.setExpanded(QStringLiteral("c1"), false);

    EXPECT_FALSE(model->data(clicking, expanded).toBool());
    EXPECT_GE(spy.count(), 1);
}

TEST(BenchmarkBreakdownModel, SurvivingGroupIdsRetainExpansionAcrossSameBenchmarkRefresh) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    vm.setExpanded(QStringLiteral("c1"), false);

    auto refreshed = makeBreakdownSnapshot(); // same benchmark id "b1"
    refreshed.projection->scenarios.front().personalBest = 999.0;
    fake->snap = refreshed;
    fake->publish();

    QAbstractItemModel *model = vm.breakdown();
    EXPECT_FALSE(model->data(model->index(1, 0), roleFor(model, "expanded")).toBool());
}

TEST(BenchmarkBreakdownModel, ResetsToExpandedDefaultsWhenSelectedBenchmarkChanges) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    vm.setExpanded(QStringLiteral("c1"), false);

    auto other = makeBreakdownSnapshot();
    other.selectedLoaded->benchmark.id = BenchmarkId{"b2"};
    other.projection->benchmarkId = BenchmarkId{"b2"};
    other.selectedId = BenchmarkId{"b2"};
    fake->snap = other;
    fake->publish();

    QAbstractItemModel *model = vm.breakdown();
    EXPECT_TRUE(model->data(model->index(1, 0), roleFor(model, "expanded")).toBool());
}

TEST(BenchmarkBreakdownModel, NodesExposesNestedValueTreeAndNotifiesOnExpansion) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    BenchmarkBreakdownModel *model = vm.breakdown();

    const QVariantList roots = model->nodes();
    ASSERT_EQ(roots.size(), 3);
    const QVariantMap clicking = roots.at(1).toMap();
    EXPECT_EQ(clicking.value("kind").toString(), "category");
    EXPECT_EQ(clicking.value("name").toString(), "Clicking");
    EXPECT_TRUE(clicking.value("expanded").toBool());
    const QVariantList clickingChildren = clicking.value("children").toList();
    ASSERT_EQ(clickingChildren.size(), 2);
    EXPECT_EQ(clickingChildren.at(0).toMap().value("kind").toString(), "scenario");

    const QSignalSpy spy(model, &BenchmarkBreakdownModel::nodesChanged);
    vm.setExpanded(QStringLiteral("c1"), false);
    EXPECT_GE(spy.count(), 1);
    EXPECT_FALSE(model->nodes().at(1).toMap().value("expanded").toBool());
}

TEST(BenchmarkBreakdownModel, IsReadOnly) {
    auto fake = breakdownFake();
    BenchmarkTrackingViewModel vm{fake};
    QAbstractItemModel *model = vm.breakdown();

    const QModelIndex clicking = model->index(1, 0);
    EXPECT_FALSE(model->flags(clicking).testFlag(Qt::ItemIsEditable));
    EXPECT_FALSE(model->setData(clicking, false, roleFor(model, "expanded")));
}
