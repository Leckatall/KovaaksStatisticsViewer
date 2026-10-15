import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

QtObject {
    component PrimitiveEditor: ComboBox {
        required property EditablePrimitiveNode node
        required property var treeModel

        Layout.fillWidth: true
        currentIndex: node && treeModel ? treeModel.primitiveMetrics.indexOf(node.metric) : -1
        model: treeModel ? treeModel.primitiveMetrics : []
        objectName: "metricComboBox"

        onActivated: if (node) node.metric = currentText
    }

    component ConstantEditor: SpinBox {
        required property EditableConstantNode node

        Layout.fillWidth: true
        from: -100000
        objectName: "constantSpinBox"
        to: 100000
        value: node ? node.value : 0

        onValueModified: if (node) node.value = value
    }

    component BinaryOpEditor: ComboBox {
        required property EditableBinaryOpNode node
        required property var kindLabel

        Layout.fillWidth: true
        currentIndex: node ? model.indexOf(node.operatorKind) : -1
        displayText: kindLabel(currentText)
        model: ["add", "subtract", "multiply", "divide"]
        objectName: "operatorComboBox"

        delegate: ItemDelegate {
            required property var modelData

            text: kindLabel(modelData)
        }

        onActivated: if (node) node.operatorKind = currentText
    }

    component RollingMeanEditor: SpinBox {
        required property EditableRollingMeanNode node

        Layout.fillWidth: true
        from: 1
        objectName: "windowSpinBox"
        to: 1000
        value: node ? node.window : 1

        onValueModified: if (node) node.window = value
    }

    component AverageAcrossRunsEditor: RowLayout {
        id: averageInstance

        required property EditableAverageAcrossRunsNode node

        ComboBox {
            currentIndex: averageInstance.node && averageInstance.node.selectionKind === "recentRuns" ? 0 : 1
            displayText: currentText === "recentRuns" ? qsTr("Recent runs") : qsTr("Top percentile")
            model: ["recentRuns", "topPercentile"]
            objectName: "selectionKindComboBox"

            onActivated: if (averageInstance.node) averageInstance.node.selectionKind = currentText
        }
        SpinBox {
            Layout.fillWidth: true
            from: 1
            objectName: "selectionValueSpinBox"
            to: 100
            value: averageInstance.node && averageInstance.node.selectionKind === "recentRuns" ? averageInstance.node.count : averageInstance.node ? averageInstance.node.percent : 1

            onValueModified: {
                if (!averageInstance.node) return;
                if (averageInstance.node.selectionKind === "recentRuns") averageInstance.node.count = value;
                else averageInstance.node.percent = value;
            }
        }
    }
}
