#include <gtest/gtest.h>

#include <QSignalSpy>

#include "editable_expression_node.h"
#include "series_expression_editor_model.h"
#include "series_expression_qml.h"

using ksv::application::PrimitiveMetric;
using ksv::application::RecentRuns;
using ksv::application::add;
using ksv::application::averageAcrossRuns;
using ksv::application::numericConstant;
using ksv::application::primitive;
using ksv::application::rollingMean;
using ksv::presentation::EditableAverageAcrossRunsNode;
using ksv::presentation::EditableBinaryOpNode;
using ksv::presentation::EditableConstantNode;
using ksv::presentation::EditableExpressionNode;
using ksv::presentation::EditablePrimitiveNode;
using ksv::presentation::EditableRollingMeanNode;
using ksv::presentation::SeriesExpressionEditorModel;
using ksv::presentation::expressionMap;
using ksv::presentation::parseExpression;

namespace {
    void primitiveRoot(SeriesExpressionEditorModel &model) { model.replaceChild(nullptr, "root", "primitive"); }

    TEST(SeriesExpressionEditorModelTest, DefaultConstructedModelHasNoRootAndNoSelection) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.root(), nullptr);
        EXPECT_EQ(model.selected(), nullptr);
        EXPECT_FALSE(model.toExpression());
    }

    TEST(SeriesExpressionEditorModelTest, LoadFromPrimitiveExpressionPopulatesRootWithLowercaseMetric) {
        SeriesExpressionEditorModel model;
        model.loadFrom(primitive(PrimitiveMetric::Hits));
        auto *root = qobject_cast<EditablePrimitiveNode *>(model.root());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->metric(), "hits");
    }

    TEST(SeriesExpressionEditorModelTest, LoadFromNestedExpressionSetsParentPointersThroughoutTheTree) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(add(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots)), 4));
        auto *root = qobject_cast<EditableRollingMeanNode *>(model.root());
        ASSERT_NE(root, nullptr);
        auto *addNode = qobject_cast<EditableBinaryOpNode *>(root->input());
        ASSERT_NE(addNode, nullptr);
        EXPECT_EQ(addNode->parentNode(), root);
        EXPECT_EQ(addNode->left()->parentNode(), addNode);
    }

    TEST(SeriesExpressionEditorModelTest, ToExpressionMapOmitsParentBookkeepingAndParses) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        const auto persistent = model.toExpressionMap();
        EXPECT_FALSE(persistent.contains("id"));
        EXPECT_TRUE(persistent.contains("primitiveMetric"));
        EXPECT_TRUE(parseExpression(persistent));
    }

    TEST(SeriesExpressionEditorModelTest, AncestorChainReturnsRootToTargetPath) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 10));
        auto *input = qobject_cast<EditableRollingMeanNode *>(model.root())->input();
        const auto path = model.ancestorChain(input);
        ASSERT_EQ(path.size(), 2);
        EXPECT_EQ(path.first().value<EditableExpressionNode *>(), model.root());
        EXPECT_EQ(path.last().value<EditableExpressionNode *>(), input);
    }

    TEST(SeriesExpressionEditorModelTest, AncestorChainOfANodeNotInTheTreeIsEmpty) {
        SeriesExpressionEditorModel model;
        model.loadFrom(primitive(PrimitiveMetric::Score));
        EditablePrimitiveNode detached;
        EXPECT_TRUE(model.ancestorChain(&detached).isEmpty());
        EXPECT_TRUE(model.ancestorChain(nullptr).isEmpty());
    }

    TEST(SeriesExpressionEditorModelTest, ReplaceChildSupportsRootBinaryUnaryAndDefaults) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "add");
        auto *root = qobject_cast<EditableBinaryOpNode *>(model.selected());
        model.replaceChild(root, "left", "constant");
        EXPECT_DOUBLE_EQ(qobject_cast<EditableConstantNode *>(root->left())->value(), 0.0);
        model.replaceChild(root, "right", "rollingMean");
        auto *rolling = qobject_cast<EditableRollingMeanNode *>(root->right());
        ASSERT_NE(rolling, nullptr);
        EXPECT_EQ(rolling->window(), 10U);
        model.replaceChild(rolling, "input", "averageAcrossRuns");
        auto *average = qobject_cast<EditableAverageAcrossRunsNode *>(rolling->input());
        ASSERT_NE(average, nullptr);
        EXPECT_EQ(average->selectionKind(), "recentRuns");
        EXPECT_EQ(average->count(), 5U);
    }

    TEST(SeriesExpressionEditorModelTest, ReplaceChildWithMismatchedSlotIsANoOpAndDoesNotLeak) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "rollingMean");
        auto *root = model.selected();
        model.replaceChild(root, "left", "primitive");
        EXPECT_EQ(qobject_cast<EditableRollingMeanNode *>(root)->input(), nullptr);
    }

    TEST(SeriesExpressionEditorModelTest, ReplaceChildWithNullParentAndNonRootSlotIsANoOp) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "left", "add");
        EXPECT_EQ(model.root(), nullptr);
        EXPECT_EQ(model.selected(), nullptr);
    }

    TEST(SeriesExpressionEditorModelTest, IncompleteUnaryNodesAreNotSaveableButCompleteNodesRoundTrip) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "rollingMean");
        EXPECT_FALSE(model.toExpression());
        model.replaceChild(model.selected(), "input", "primitive");
        const auto parsed = model.toExpression();
        ASSERT_TRUE(parsed);
        EXPECT_EQ(expressionMap(*parsed).value("window").toUInt(), 10U);

        SeriesExpressionEditorModel average;
        average.replaceChild(nullptr, "root", "averageAcrossRuns");
        EXPECT_FALSE(average.toExpression());
        average.replaceChild(average.selected(), "input", "primitive");
        EXPECT_TRUE(average.toExpression());
    }

    TEST(SeriesExpressionEditorModelTest, DeleteNodeClearsRootOrChildAndSelectsParent) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "add");
        auto *root = qobject_cast<EditableBinaryOpNode *>(model.selected());
        model.replaceChild(root, "left", "primitive");
        model.deleteNode(root->left());
        EXPECT_EQ(root->left(), nullptr);
        EXPECT_EQ(model.selected(), root);
        model.deleteNode(nullptr);
        EXPECT_EQ(model.root(), root);
        model.deleteNode(root);
        EXPECT_EQ(model.root(), nullptr);
        EXPECT_EQ(model.selected(), nullptr);
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedPreservesNodeAndUsesSharedDefaults) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        auto *originalPrimitive = model.root();
        model.wrapSelected("rollingMean");
        auto *rolling = qobject_cast<EditableRollingMeanNode *>(model.root());
        ASSERT_NE(rolling, nullptr);
        EXPECT_EQ(rolling->window(), 10U);
        EXPECT_EQ(rolling->input(), originalPrimitive);
        model.wrapSelected("add");
        auto *added = qobject_cast<EditableBinaryOpNode *>(model.root());
        ASSERT_NE(added, nullptr);
        EXPECT_EQ(added->left(), rolling);
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedWithNoSelectionIsANoOp) {
        SeriesExpressionEditorModel model;
        model.wrapSelected("rollingMean");
        EXPECT_EQ(model.root(), nullptr);
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedWithALeafKindPreservesRootAndSelection) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        auto *originalPrimitive = model.root();
        model.wrapSelected("constant");
        EXPECT_EQ(model.root(), originalPrimitive);
        EXPECT_EQ(model.selected(), originalPrimitive);
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedOnARightChildRestoresItToTheRightSlot) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "add");
        auto *root = qobject_cast<EditableBinaryOpNode *>(model.selected());
        model.replaceChild(root, "left", "primitive");
        model.replaceChild(root, "right", "constant");
        model.select(root->right());
        model.wrapSelected("rollingMean");
        EXPECT_NE(root->right(), nullptr);
        EXPECT_EQ(qobject_cast<EditableRollingMeanNode *>(root->right())->kind(), "rollingMean");
        ASSERT_NE(root->left(), nullptr);
        EXPECT_EQ(root->left()->kind(), "primitive");
    }

    TEST(SeriesExpressionEditorModelTest, SameShapeOperatorChangeIsAPropertyWriteThatPreservesChildren) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "add");
        auto *root = qobject_cast<EditableBinaryOpNode *>(model.selected());
        model.replaceChild(root, "left", "primitive");
        model.replaceChild(root, "right", "constant");
        auto *left = root->left();
        root->setKind("divide");
        EXPECT_EQ(root->kind(), "divide");
        EXPECT_EQ(root->left(), left);
    }

    TEST(SeriesExpressionEditorModelTest, DescribeDelegatesToTheNodeAndHandlesNull) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.describe(nullptr), QString::fromUtf8("…"));
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 9));
        EXPECT_EQ(model.describe(model.root()), "RollingMean(score, window: 9)");
    }

    TEST(SeriesExpressionEditorModelTest, MetadataMatchesTheEditorContract) {
        SeriesExpressionEditorModel model;
        EXPECT_TRUE(model.isBinary("multiply"));
        EXPECT_FALSE(model.isBinary("rollingMean"));
        EXPECT_EQ(model.childSlotsFor("add"), (QStringList{"left", "right"}));
        EXPECT_EQ(model.childSlotsFor("rollingMean"), (QStringList{"input"}));
        EXPECT_TRUE(model.childSlotsFor("constant").isEmpty());
        EXPECT_EQ(model.nodeKinds().size(), 11);
        EXPECT_EQ(model.primitiveMetrics(), (QStringList{"score", "shots", "hits", "kills", "dmg"}));
    }

    TEST(SeriesExpressionEditorModelTest, SelectSetsTheSelectedProperty) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 3));
        auto *input = qobject_cast<EditableRollingMeanNode *>(model.root())->input();
        model.select(input);
        EXPECT_EQ(model.selected(), input);
    }

    TEST(SeriesExpressionEditorModelTest, ToExpressionRoundTripsEverySupportedNodeKind) {
        const auto expression = averageAcrossRuns(
            rollingMean(add(primitive(PrimitiveMetric::Score), numericConstant(2.0)), 5), RecentRuns{3});
        SeriesExpressionEditorModel model;
        model.loadFrom(expression);
        const auto result = model.toExpression();
        ASSERT_TRUE(result);
        EXPECT_EQ(expressionMap(*result), expressionMap(expression));
    }

    TEST(SeriesExpressionEditorModelTest, RootChangedAndSelectedChangedFireOnlyWhenTheValueActuallyChanges) {
        SeriesExpressionEditorModel model;
        QSignalSpy rootSpy(&model, &SeriesExpressionEditorModel::rootChanged);
        QSignalSpy selectedSpy(&model, &SeriesExpressionEditorModel::selectedChanged);
        model.replaceChild(nullptr, "root", "primitive");
        EXPECT_EQ(rootSpy.count(), 1);
        EXPECT_EQ(selectedSpy.count(), 1);
        model.select(model.root());
        EXPECT_EQ(selectedSpy.count(), 1);
    }

    TEST(SeriesExpressionEditorModelTest, ReplaceChildClearsSelectedBeforeTheOldSubtreeIsDestroyed) {
        SeriesExpressionEditorModel model;
        model.replaceChild(nullptr, "root", "rollingMean");
        auto *root = qobject_cast<EditableRollingMeanNode *>(model.selected());
        model.replaceChild(root, "input", "primitive");
        auto *innerNode = root->input();
        model.select(innerNode);
        bool sawDanglingSelection = false;
        QObject::connect(&model, &SeriesExpressionEditorModel::selectedChanged, &model, [&] {
            sawDanglingSelection = sawDanglingSelection || model.selected() == innerNode;
        });
        model.replaceChild(nullptr, "root", "constant");
        EXPECT_FALSE(sawDanglingSelection);
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedOnTheRootFiresRootChangedExactlyOnce) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        QSignalSpy rootSpy(&model, &SeriesExpressionEditorModel::rootChanged);
        model.wrapSelected("rollingMean");
        EXPECT_EQ(rootSpy.count(), 1);
        EXPECT_NE(model.root(), nullptr);
    }
}
