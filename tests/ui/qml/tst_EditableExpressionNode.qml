import QtQuick
import QtTest
import KovaaksStatsViewer

TestCase {
    name: "EditableExpressionNode"

    function test_rollingMeanNodeConstructsWithGivenWindow() {
        const node = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableRollingMeanNode { window: 10 }',
            this, "inline")
        compare(node.window, 10)
        compare(node.kind, "rollingMean")
    }

    function test_binaryOpNodeOperatorKindIsWritableFromQml() {
        const node = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableBinaryOpNode { operatorKind: "multiply" }',
            this, "inline")
        compare(node.operatorKind, "multiply")
        compare(node.kind, "multiply")
    }

    function test_unaryOpNodeOperatorKindIsWritableFromQml() {
        const node = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableUnaryOpNode { operatorKind: "projectRateToFinal" }',
            this, "inline")
        compare(node.operatorKind, "projectRateToFinal")
    }

    function test_remainingConcreteNodeTypesResolveFromQml() {
        const primitive = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditablePrimitiveNode { metric: "kills" }', this, "inline")
        compare(primitive.metric, "kills")

        const constant = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableConstantNode { value: 3.5 }', this, "inline")
        compare(constant.value, 3.5)

        const rollingMean = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableRollingMeanNode {}', this, "inline")
        compare(rollingMean.kind, "rollingMean")

        const average = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableAverageAcrossRunsNode { count: 7 }', this, "inline")
        compare(average.count, 7)
    }

    function test_nestedChildConstructedInQmlSurvivesGarbageCollection() {
        const node = Qt.createQmlObject(
            'import KovaaksStatsViewer; EditableBinaryOpNode { left: EditablePrimitiveNode { metric: "kills" } }',
            this, "inline")
        verify(node.left !== null)
        compare(node.left.metric, "kills")

        gc()
        verify(node.left !== null)
        compare(node.left.metric, "kills")
    }
}
