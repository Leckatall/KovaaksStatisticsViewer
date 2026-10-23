#include <gtest/gtest.h>

#include <array>

#include <QSignalSpy>

#include "app/contracts/expression_dsl.h"
#include "editable_expression_node.h"
#include "series_expression_editor_model.h"

using ksv::application::PrimitiveMetric;
using ksv::application::kPrimitiveMetrics;
using ksv::application::RecentRuns;
using ksv::application::TopPercentile;
using ksv::application::add;
using ksv::application::averageAcrossRuns;
using ksv::application::bestRun;
using ksv::application::finalValue;
using ksv::application::FinalStat;
using ksv::application::kFinalStats;
using ksv::application::divide;
using ksv::application::encodeExpressionDsl;
using ksv::application::multiply;
using ksv::application::numericConstant;
using ksv::application::primitive;
using ksv::application::projectRateToFinal;
using ksv::application::projectedFinalValue;
using ksv::application::rollingMean;
using ksv::application::runningSum;
using ksv::application::subtract;
using ksv::presentation::EditableAverageAcrossRunsNode;
using ksv::presentation::EditableBinaryOpNode;
using ksv::presentation::EditableConstantNode;
using ksv::presentation::EditableExpressionNode;
using ksv::presentation::EditableFinalValueNode;
using ksv::presentation::EditableUnaryOpNode;
using ksv::presentation::EditablePrimitiveNode;
using ksv::presentation::EditableRollingMeanNode;
using ksv::presentation::SeriesExpressionEditorModel;

namespace {
    void primitiveRoot(SeriesExpressionEditorModel &model) { model.replaceChild(nullptr, "root", "primitive"); }

    TEST(SeriesExpressionEditorModelTest, ToDslTextEncodesTheCurrentExpression) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(add(primitive(PrimitiveMetric::Hits), numericConstant(2.0)), 4));
        EXPECT_EQ(model.toDslText(), "RollingMean(window: 4, Add(HITS, 2))");
    }

    TEST(SeriesExpressionEditorModelTest, ToDslTextIsEmptyWhenExpressionIncomplete) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.toDslText(), QString());
    }

    TEST(SeriesExpressionEditorModelTest, ApplyDslTextRebuildsTheTreeFromValidText) {
        SeriesExpressionEditorModel model;
        EXPECT_TRUE(model.applyDslText("RollingMean(window: 4, HITS)"));
        auto *root = qobject_cast<EditableRollingMeanNode *>(model.root());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->window(), 4U);
        EXPECT_EQ(qobject_cast<EditablePrimitiveNode *>(root->input())->metric(), "hits");
    }

    TEST(SeriesExpressionEditorModelTest, DslTextPropertyReflectsTheCurrentExpression) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 4));
        EXPECT_EQ(model.property("dslText").toString(), "RollingMean(window: 4, SCORE)");
    }

    TEST(SeriesExpressionEditorModelTest, DslTextChangedFiresWhenAFieldEditMutatesANode) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 4));
        QSignalSpy spy(&model, &SeriesExpressionEditorModel::dslTextChanged);

        qobject_cast<EditableRollingMeanNode *>(model.root())->setWindow(9);

        EXPECT_GE(spy.count(), 1);
        EXPECT_EQ(model.toDslText(), "RollingMean(window: 9, SCORE)");
    }

    TEST(SeriesExpressionEditorModelTest, DslTextChangedFiresOnStructuralMutation) {
        SeriesExpressionEditorModel model;
        QSignalSpy spy(&model, &SeriesExpressionEditorModel::dslTextChanged);
        model.replaceChild(nullptr, "root", "primitive");
        EXPECT_GE(spy.count(), 1);
    }

    TEST(SeriesExpressionEditorModelTest, ApplyDslTextLeavesTreeUnchangedOnInvalidText) {
        SeriesExpressionEditorModel model;
        model.loadFrom(primitive(PrimitiveMetric::Score));
        auto *before = model.root();
        EXPECT_FALSE(model.applyDslText("not a valid expression("));
        EXPECT_EQ(model.root(), before);
    }

    TEST(SeriesExpressionEditorModelTest, DefaultConstructedModelHasNoRootAndNoSelection) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.root(), nullptr);
        EXPECT_EQ(model.selected(), nullptr);
        EXPECT_FALSE(model.toExpression());
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
        EXPECT_EQ(encodeExpressionDsl(*parsed), "RollingMean(window: 10, SCORE)");

        SeriesExpressionEditorModel average;
        average.replaceChild(nullptr, "root", "averageAcrossRuns");
        EXPECT_FALSE(average.toExpression());
        average.replaceChild(average.selected(), "input", "primitive");
        EXPECT_TRUE(average.toExpression());

        SeriesExpressionEditorModel binary;
        binary.replaceChild(nullptr, "root", "add");
        auto *addNode = qobject_cast<EditableBinaryOpNode *>(binary.root());
        binary.replaceChild(addNode, "left", "primitive");
        EXPECT_FALSE(binary.toExpression());
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

    TEST(SeriesExpressionEditorModelTest, DescribeDelegatesToTheNodeAndHandlesNull) {
        SeriesExpressionEditorModel model;
        EXPECT_EQ(model.describe(nullptr), QString::fromUtf8("…"));
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 9));
        EXPECT_EQ(model.describe(model.root()), "RollingMean(window: 9, score)");
    }

    TEST(SeriesExpressionEditorModelTest, MetadataMatchesTheEditorContract) {
        SeriesExpressionEditorModel model;
        EXPECT_TRUE(model.isBinary("multiply"));
        EXPECT_FALSE(model.isBinary("rollingMean"));
        EXPECT_EQ(model.childSlotsFor("add"), (QStringList{"left", "right"}));
        EXPECT_EQ(model.childSlotsFor("rollingMean"), (QStringList{"input"}));
        EXPECT_TRUE(model.childSlotsFor("constant").isEmpty());

        for (const auto &kind: model.nodeKinds()) {
            SeriesExpressionEditorModel fresh;
            fresh.replaceChild(nullptr, "root", kind);
            EXPECT_NE(fresh.root(), nullptr) << kind.toStdString();
        }
        // The metric list's order is the index nodeToExpression() maps back through kPrimitiveMetrics.
        ASSERT_EQ(model.primitiveMetrics().size(), static_cast<qsizetype>(kPrimitiveMetrics.size()));
        for (const auto metric: kPrimitiveMetrics) {
            SeriesExpressionEditorModel loaded;
            loaded.loadFrom(primitive(metric));
            const auto result = loaded.toExpression();
            ASSERT_TRUE(result);
            EXPECT_EQ(encodeExpressionDsl(*result), encodeExpressionDsl(primitive(metric)));
        }
    }

    TEST(SeriesExpressionEditorModelTest, SelectSetsTheSelectedProperty) {
        SeriesExpressionEditorModel model;
        model.loadFrom(rollingMean(primitive(PrimitiveMetric::Score), 3));
        auto *input = qobject_cast<EditableRollingMeanNode *>(model.root())->input();
        model.select(input);
        EXPECT_EQ(model.selected(), input);
    }

    TEST(SeriesExpressionEditorModelTest, ToExpressionRoundTripsEverySupportedNodeKind) {
        const std::array expressions{
            primitive(PrimitiveMetric::Score),
            numericConstant(2.0),
            add(primitive(PrimitiveMetric::Score), numericConstant(2.0)),
            subtract(primitive(PrimitiveMetric::Score), primitive(PrimitiveMetric::Hits)),
            multiply(primitive(PrimitiveMetric::Score), numericConstant(0.5)),
            divide(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots)),
            runningSum(primitive(PrimitiveMetric::Score)),
            projectedFinalValue(primitive(PrimitiveMetric::Score)),
            projectRateToFinal(primitive(PrimitiveMetric::Score)),
            rollingMean(primitive(PrimitiveMetric::Score), 5),
            averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{3}),
            averageAcrossRuns(primitive(PrimitiveMetric::Hits), TopPercentile{12.5}),
            finalValue(FinalStat::Misses),
            bestRun(finalValue(FinalStat::Dmg)),
            subtract(runningSum(primitive(PrimitiveMetric::Score)), bestRun(runningSum(primitive(PrimitiveMetric::Score)))),
        };
        for (const auto &expression: expressions) {
            SeriesExpressionEditorModel model;
            model.loadFrom(expression);
            const auto result = model.toExpression();
            ASSERT_TRUE(result);
            EXPECT_EQ(encodeExpressionDsl(*result), encodeExpressionDsl(expression));
        }
    }

    TEST(SeriesExpressionEditorModelTest, FinalStatListOrderMatchesKFinalStats) {
        SeriesExpressionEditorModel model;
        ASSERT_EQ(model.finalStats().size(), static_cast<qsizetype>(kFinalStats.size()));
        for (const auto stat: kFinalStats) {
            SeriesExpressionEditorModel loaded;
            loaded.loadFrom(finalValue(stat));
            const auto *node = qobject_cast<EditableFinalValueNode *>(loaded.root());
            ASSERT_NE(node, nullptr);
            EXPECT_EQ(model.finalStats().indexOf(node->stat()), static_cast<int>(stat));
            EXPECT_EQ(loaded.toDslText(), QString::fromStdString(encodeExpressionDsl(finalValue(stat))));
        }
    }

    TEST(SeriesExpressionEditorModelTest, WrapSelectedInBestRunUsesTheUnaryPath) {
        SeriesExpressionEditorModel model;
        model.loadFrom(runningSum(primitive(PrimitiveMetric::Score)));
        model.select(model.root());
        model.wrapSelected("bestRun");
        auto *wrapper = qobject_cast<EditableUnaryOpNode *>(model.root());
        ASSERT_NE(wrapper, nullptr);
        EXPECT_EQ(wrapper->kind(), "bestRun");
        ASSERT_NE(wrapper->input(), nullptr);
        EXPECT_EQ(wrapper->input()->kind(), "runningSum");
        EXPECT_EQ(model.toDslText(), "BestRun(RunningSum(SCORE))");
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
