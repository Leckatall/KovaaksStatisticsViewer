#include <gtest/gtest.h>

#include "series_expression_editor_model.h"
#include "series_expression_qml.h"

using ksv::application::PrimitiveMetric;
using ksv::application::add;
using ksv::application::averageAcrossRuns;
using ksv::application::numericConstant;
using ksv::application::primitive;
using ksv::application::rollingMean;
using ksv::application::RecentRuns;
using ksv::presentation::SeriesExpressionEditorModel;
using ksv::presentation::expressionMap;
using ksv::presentation::parseExpression;

namespace {
    QVariantMap child(const QVariantMap &node, const char *slot) { return node.value(slot).toMap(); }

    void primitiveRoot(SeriesExpressionEditorModel &model) { model.replaceChild({}, "root", "primitive"); }

    TEST(SeriesExpressionEditorModelTest, DefaultConstructedModelHasEmptyRootAndNoSelection) {
        SeriesExpressionEditorModel model;
        EXPECT_TRUE(model.root().isEmpty());
        EXPECT_TRUE(model.selectedNodeId().isEmpty());
        EXPECT_FALSE(model.toExpression());
    }

    TEST(SeriesExpressionEditorModelTest, LoadFromPrimitiveExpressionPopulatesRootWithLowercaseMetric) {
        SeriesExpressionEditorModel model;
        model.loadFrom(primitive(PrimitiveMetric::Hits));
        EXPECT_EQ(model.root().value("metric"), "hits");
    }

    TEST(SeriesExpressionEditorModelTest, LoadFromNestedExpressionAssignsEveryNodeAUniqueId) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(add(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots)), 4));
        const auto root = model.root();
        EXPECT_NE(root.value("id"), child(root, "input").value("id"));
        EXPECT_NE(child(root, "input").value("id"), child(child(root, "input"), "left").value("id"));
    }

    TEST(SeriesExpressionEditorModelTest, MapShapesKeepEditorIdsSeparateFromPersistence) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        const auto editable = model.root();
        const auto persistent = model.toExpressionMap();
        EXPECT_TRUE(editable.contains("id"));
        EXPECT_TRUE(editable.contains("metric"));
        EXPECT_FALSE(editable.contains("primitiveMetric"));
        EXPECT_FALSE(persistent.contains("id"));
        EXPECT_TRUE(persistent.contains("primitiveMetric"));
        EXPECT_TRUE(parseExpression(persistent));
    }

    TEST(SeriesExpressionEditorModelTest, AncestorChainReturnsEditableShapeNodesWithIdAndMetricField) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 10));
        const auto input = child(model.root(), "input");
        const auto path = model.ancestorChain(input.value("id").toString());
        ASSERT_EQ(path.size(), 2);
        EXPECT_TRUE(path.last().toMap().contains("id"));
        EXPECT_TRUE(path.last().toMap().contains("metric"));
    }

    TEST(SeriesExpressionEditorModelTest, ToExpressionMapNestedShapeParsesSuccessfully) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "rollingMean");
        model.replaceChild(model.selectedNodeId(), "input", "primitive");
        const auto map = model.toExpressionMap();
        EXPECT_FALSE(map.contains("id"));
        EXPECT_FALSE(child(map, "input").contains("id"));
        EXPECT_TRUE(parseExpression(map));
    }

    TEST(SeriesExpressionEditorModelTest, ReplaceChildSupportsRootBinaryUnaryAndDefaults) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "add");
        const auto rootId = model.selectedNodeId();
        model.replaceChild(rootId, "left", "constant");
        EXPECT_DOUBLE_EQ(child(model.root(), "left").value("value").toDouble(), 0.0);
        model.replaceChild(rootId, "right", "rollingMean");
        const auto rolling = child(model.root(), "right");
        EXPECT_EQ(rolling.value("window").toUInt(), 10U);
        const auto before = model.treeRevision();
        model.replaceChild(rolling.value("id").toString(), "input", "averageAcrossRuns");
        EXPECT_GT(model.treeRevision(), before);
        const auto average = child(child(model.root(), "right"), "input");
        EXPECT_EQ(average.value("selection").toMap().value("kind"), "recentRuns");
        EXPECT_EQ(average.value("selection").toMap().value("count").toUInt(), 5U);
    }

    TEST(SeriesExpressionEditorModelTest, IncompleteUnaryNodesAreNotSaveableButCompleteNodesRoundTrip) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "rollingMean");
        EXPECT_FALSE(model.toExpression());
        const auto rollingId = model.selectedNodeId();
        model.replaceChild(rollingId, "input", "primitive");
        const auto parsed = model.toExpression();
        ASSERT_TRUE(parsed);
        EXPECT_EQ(expressionMap(*parsed).value("window").toUInt(), 10U);

        SeriesExpressionEditorModel average;
        average.replaceChild({}, "root", "averageAcrossRuns");
        EXPECT_FALSE(average.toExpression());
        average.replaceChild(average.selectedNodeId(), "input", "primitive");
        EXPECT_TRUE(average.toExpression());
    }

    TEST(SeriesExpressionEditorModelTest, DeleteNodeClearsRootOrChildAndUnknownIsNoOp) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "add");
        const auto rootId = model.selectedNodeId();
        model.replaceChild(rootId, "left", "primitive");
        model.deleteNode(child(model.root(), "left").value("id").toString());
        EXPECT_TRUE(child(model.root(), "left").isEmpty());
        EXPECT_EQ(model.selectedNodeId(), rootId);
        const auto revision = model.treeRevision();
        model.deleteNode("missing");
        EXPECT_EQ(model.treeRevision(), revision);
        model.deleteNode(rootId);
        EXPECT_TRUE(model.root().isEmpty());
        EXPECT_TRUE(model.selectedNodeId().isEmpty());
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedPreservesNodeAndUsesSharedDefaults) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        model.wrapSelected("rollingMean");
        const auto rolling = model.root();
        EXPECT_EQ(rolling.value("window").toUInt(), 10U);
        EXPECT_EQ(child(rolling, "input").value("kind"), "primitive");
        model.wrapSelected("add");
        EXPECT_EQ(model.root().value("kind"), "add");
        EXPECT_EQ(child(model.root(), "left").value("kind"), "rollingMean");
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedWithNoSelectionIsANoOp) {
        SeriesExpressionEditorModel model;
        const auto revision = model.treeRevision();

        model.wrapSelected("rollingMean");

        EXPECT_TRUE(model.root().isEmpty());
        EXPECT_EQ(model.treeRevision(), revision);
    }

    TEST(SeriesExpressionEditorModelTest, ChangeBinaryOperatorPreservesChildrenAndRejectsInvalidTargets) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "add");
        const auto id = model.selectedNodeId();
        model.replaceChild(id, "left", "primitive");
        model.replaceChild(id, "right", "constant");
        model.changeBinaryOperator(id, "divide");
        EXPECT_EQ(model.root().value("kind"), "divide");
        EXPECT_EQ(child(model.root(), "left").value("kind"), "primitive");
        model.changeBinaryOperator(id, "rollingMean");
        model.changeBinaryOperator("missing", "add");
        EXPECT_EQ(model.root().value("kind"), "divide");
    }

    TEST(SeriesExpressionEditorModelTest, UpdateFieldAndSelectionKindChangeTheCorrectFields) {
        SeriesExpressionEditorModel model;
        model.replaceChild({}, "root", "constant");
        model.updateField(model.selectedNodeId(), "value", 3.5);
        EXPECT_DOUBLE_EQ(model.root().value("value").toDouble(), 3.5);
        primitiveRoot(model);
        model.updateField(model.selectedNodeId(), "metric", "kills");
        EXPECT_EQ(model.root().value("metric"), "kills");
        model.wrapSelected("rollingMean");
        model.updateField(model.selectedNodeId(), "window", 7);
        EXPECT_EQ(model.root().value("window").toUInt(), 7U);
        model.wrapSelected("averageAcrossRuns");
        model.updateField(model.selectedNodeId(), "count", 8);
        EXPECT_EQ(model.root().value("selection").toMap().value("count").toUInt(), 8U);
        model.changeSelectionKind(model.selectedNodeId(), "topPercentile");
        model.updateField(model.selectedNodeId(), "percent", 25.0);
        EXPECT_DOUBLE_EQ(model.root().value("selection").toMap().value("percent").toDouble(), 25.0);
        model.changeSelectionKind(model.selectedNodeId(), "recentRuns");
        EXPECT_EQ(model.root().value("selection").toMap().value("count").toUInt(), 5U);
    }

    TEST(SeriesExpressionEditorModelTest, ChangeSelectionKindIgnoresNonAverageAcrossRunsNode) {
        SeriesExpressionEditorModel model;
        primitiveRoot(model);
        const auto before = model.root();

        model.changeSelectionKind(model.selectedNodeId(), "topPercentile");

        EXPECT_EQ(model.root(), before);
    }

    TEST(SeriesExpressionEditorModelTest, DescribeAndMetadataMatchTheEditorContract) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.describe({}), QString::fromUtf8("…"));
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 9));
        EXPECT_EQ(model.describe(model.root()), "RollingMean(score, window: 9)");
        EXPECT_TRUE(model.isBinary("multiply"));
        EXPECT_FALSE(model.isBinary("rollingMean"));
        EXPECT_EQ(model.childSlotsFor("add"), (QStringList{"left", "right"}));
        EXPECT_EQ(model.childSlotsFor("rollingMean"), (QStringList{"input"}));
        EXPECT_TRUE(model.childSlotsFor("constant").isEmpty());
        EXPECT_EQ(model.nodeKinds().size(), 11);
        EXPECT_EQ(model.primitiveMetrics(), (QStringList{"score", "shots", "hits", "kills", "dmg"}));
    }

    TEST(SeriesExpressionEditorModelTest, AncestorChainAndSelectFollowTheRootToTargetPath) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 3));
        const auto inputId = child(model.root(), "input").value("id").toString();
        EXPECT_EQ(model.ancestorChain(inputId).size(), 2);
        EXPECT_TRUE(model.ancestorChain("missing").isEmpty());
        model.select(inputId);
        EXPECT_EQ(model.selectedNodeId(), inputId);
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
}
