#include <QPointer>
#include <QSignalSpy>
#include <gtest/gtest.h>

#include "editable_expression_node.h"

using ksv::presentation::EditableAverageAcrossRunsNode;
using ksv::presentation::EditableBinaryOpNode;
using ksv::presentation::EditableConstantNode;
using ksv::presentation::EditableExpressionNode;
using ksv::presentation::EditablePrimitiveNode;
using ksv::presentation::EditableRollingMeanNode;
using ksv::presentation::EditableUnaryOpNode;

namespace {
    TEST(EditablePrimitiveNodeTest, DefaultsToScoreAndReportsItsOwnKind) {
        EditablePrimitiveNode node;
        EXPECT_EQ(node.kind(), "primitive");
        EXPECT_EQ(node.metric(), "score");
        EXPECT_EQ(node.describe(), "score");
    }

    TEST(EditablePrimitiveNodeTest, SetMetricEmitsMetricChanged) {
        EditablePrimitiveNode node;
        QSignalSpy spy(&node, &EditablePrimitiveNode::metricChanged);
        node.setMetric("kills");
        EXPECT_EQ(node.metric(), "kills");
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableConstantNodeTest, DefaultsToZeroAndReportsItsOwnKind) {
        EditableConstantNode node;
        EXPECT_EQ(node.kind(), "constant");
        EXPECT_DOUBLE_EQ(node.value(), 0.0);
        EXPECT_EQ(node.describe(), "0");
    }

    TEST(EditableConstantNodeTest, SetValueEmitsValueChanged) {
        EditableConstantNode node;
        QSignalSpy spy(&node, &EditableConstantNode::valueChanged);
        node.setValue(3.5);
        EXPECT_DOUBLE_EQ(node.value(), 3.5);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableBinaryOpNodeTest, DefaultsToAddAndTakesOwnershipOfChildren) {
        auto *node = new EditableBinaryOpNode();
        EXPECT_EQ(node->kind(), "add");
        node->setLeft(new EditablePrimitiveNode());
        node->setRight(new EditableConstantNode());
        ASSERT_NE(node->left(), nullptr);
        ASSERT_NE(node->right(), nullptr);
        EXPECT_EQ(node->left()->parentNode(), node);
        EXPECT_EQ(node->left()->kind(), "primitive");
        EXPECT_EQ(node->right()->kind(), "constant");
        delete node;
    }

    TEST(EditableBinaryOpNodeTest, SetKindChangesOperatorWithoutTouchingChildren) {
        EditableBinaryOpNode node;
        node.setLeft(new EditablePrimitiveNode());
        auto *left = node.left();
        QSignalSpy spy(&node, &EditableBinaryOpNode::kindChanged);
        node.setKind("multiply");
        EXPECT_EQ(node.kind(), "multiply");
        EXPECT_EQ(node.left(), left);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableBinaryOpNodeTest, ReplacingLeftDestroysThePreviousChild) {
        EditableBinaryOpNode node;
        node.setLeft(new EditablePrimitiveNode());
        QPointer<EditableExpressionNode> first_left(node.left());
        node.setLeft(new EditableConstantNode());
        EXPECT_TRUE(first_left.isNull());
    }

    TEST(EditableBinaryOpNodeTest, SetLeftAndSetRightEmitTheirOwnNotifySignal) {
        EditableBinaryOpNode node;
        QSignalSpy left_spy(&node, &EditableBinaryOpNode::leftChanged);
        QSignalSpy right_spy(&node, &EditableBinaryOpNode::rightChanged);
        node.setLeft(new EditablePrimitiveNode());
        EXPECT_EQ(left_spy.count(), 1);
        EXPECT_EQ(right_spy.count(), 0);
        node.setRight(new EditableConstantNode());
        EXPECT_EQ(left_spy.count(), 1);
        EXPECT_EQ(right_spy.count(), 1);
    }

    TEST(EditableBinaryOpNodeTest, TakeLeftReleasesOwnershipWithoutDestroyingTheChild) {
        EditableBinaryOpNode node;
        node.setLeft(new EditablePrimitiveNode());
        QPointer<EditableExpressionNode> left(node.left());
        QSignalSpy spy(&node, &EditableBinaryOpNode::leftChanged);
        auto detached = node.takeLeft();
        EXPECT_EQ(node.left(), nullptr);
        ASSERT_FALSE(left.isNull());
        EXPECT_EQ(detached.get(), left.data());
        EXPECT_EQ(detached->parentNode(), nullptr);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableBinaryOpNodeTest, AssigningAnAlreadyAttachedSiblingToTheOtherSlotIsRejected) {
        EditableBinaryOpNode node;
        node.setLeft(new EditablePrimitiveNode());
        auto *left = node.left();
        QSignalSpy spy(&node, &EditableBinaryOpNode::rightChanged);
        node.setRight(left);
        EXPECT_EQ(node.right(), nullptr);
        EXPECT_EQ(node.left(), left);
        EXPECT_EQ(spy.count(), 0);
    }

    TEST(EditableBinaryOpNodeTest, ReattachingAChildFromAnotherParentWithoutTakeFirstIsRejected) {
        EditableBinaryOpNode node_a;
        EditableBinaryOpNode node_b;
        node_a.setLeft(new EditablePrimitiveNode());
        auto *child = node_a.left();
        node_b.setLeft(child);
        EXPECT_EQ(node_b.left(), nullptr);
        EXPECT_EQ(node_a.left(), child);
    }

    TEST(EditableBinaryOpNodeTest, ReattachingAChildAfterTakeFirstSucceeds) {
        EditableBinaryOpNode node_a;
        EditableBinaryOpNode node_b;
        node_a.setLeft(new EditablePrimitiveNode());
        auto detached = node_a.takeLeft();
        auto *child = detached.get();
        node_b.setLeft(detached.release());
        EXPECT_EQ(node_b.left(), child);
        EXPECT_EQ(child->parentNode(), &node_b);
    }

    TEST(EditableUnaryOpNodeTest, DefaultsToRunningSumAndOwnsInput) {
        EditableUnaryOpNode node;
        EXPECT_EQ(node.kind(), "runningSum");
        node.setInput(new EditablePrimitiveNode());
        ASSERT_NE(node.input(), nullptr);
        EXPECT_EQ(node.input()->parentNode(), &node);
    }

    TEST(EditableUnaryOpNodeTest, SetInputEmitsInputChanged) {
        EditableUnaryOpNode node;
        QSignalSpy spy(&node, &EditableUnaryOpNode::inputChanged);
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableUnaryOpNodeTest, SetKindChangesOperator) {
        EditableUnaryOpNode node;
        node.setKind("projectRateToFinal");
        EXPECT_EQ(node.kind(), "projectRateToFinal");
    }

    TEST(EditableBinaryOpNodeTest, OperatorKindIsWritableAsAQmlProperty) {
        EditableBinaryOpNode node;
        EXPECT_TRUE(node.setProperty("operatorKind", "multiply"));
        EXPECT_EQ(node.kind(), "multiply");
    }

    TEST(EditableRollingMeanNodeTest, DefaultsToWindowTenAndOwnsInput) {
        EditableRollingMeanNode node;
        EXPECT_EQ(node.kind(), "rollingMean");
        EXPECT_EQ(node.window(), 10U);
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(node.input()->parentNode(), &node);
    }

    TEST(EditableRollingMeanNodeTest, SetWindowEmitsWindowChanged) {
        EditableRollingMeanNode node;
        QSignalSpy spy(&node, &EditableRollingMeanNode::windowChanged);
        node.setWindow(7);
        EXPECT_EQ(node.window(), 7U);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableRollingMeanNodeTest, SetInputEmitsInputChanged) {
        EditableRollingMeanNode node;
        QSignalSpy spy(&node, &EditableRollingMeanNode::inputChanged);
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableRollingMeanNodeTest, DescribeIncludesWindowAndInputDescription) {
        EditableRollingMeanNode node;
        node.setWindow(9);
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(node.describe(), "RollingMean(window: 9, score)");
    }

    TEST(EditableAverageAcrossRunsNodeTest, DefaultsToRecentRunsCountFive) {
        EditableAverageAcrossRunsNode node;
        EXPECT_EQ(node.kind(), "averageAcrossRuns");
        EXPECT_EQ(node.selectionKind(), "recentRuns");
        EXPECT_EQ(node.count(), 5U);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SetSelectionKindEmitsSelectionKindChanged) {
        EditableAverageAcrossRunsNode node;
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::selectionKindChanged);
        node.setSelectionKind("topPercentile");
        EXPECT_EQ(node.selectionKind(), "topPercentile");
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SwitchingToTopPercentileResetsPercentToTenAndEmitsPercentChanged) {
        EditableAverageAcrossRunsNode node;
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::percentChanged);
        node.setSelectionKind("topPercentile");
        EXPECT_DOUBLE_EQ(node.percent(), 10.0);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SwitchingBackToRecentRunsResetsCountToFiveAndEmitsCountChanged) {
        EditableAverageAcrossRunsNode node;
        node.setSelectionKind("topPercentile");
        node.setCount(99);
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::countChanged);
        node.setSelectionKind("recentRuns");
        EXPECT_EQ(node.count(), 5U);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SetCountEmitsCountChanged) {
        EditableAverageAcrossRunsNode node;
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::countChanged);
        node.setCount(8);
        EXPECT_EQ(node.count(), 8U);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SetPercentEmitsPercentChanged) {
        EditableAverageAcrossRunsNode node;
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::percentChanged);
        node.setPercent(25.0);
        EXPECT_DOUBLE_EQ(node.percent(), 25.0);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, SetInputEmitsInputChanged) {
        EditableAverageAcrossRunsNode node;
        QSignalSpy spy(&node, &EditableAverageAcrossRunsNode::inputChanged);
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(spy.count(), 1);
    }

    TEST(EditableAverageAcrossRunsNodeTest, DescribeReflectsRecentRunsOrTopPercentile) {
        EditableAverageAcrossRunsNode node;
        node.setInput(new EditablePrimitiveNode());
        EXPECT_EQ(node.describe(), "AverageAcrossRuns(over: recent 5, score)");
        node.setSelectionKind("topPercentile");
        node.setPercent(15.0);
        EXPECT_EQ(node.describe(), "AverageAcrossRuns(over: top 15%, score)");
    }
}
