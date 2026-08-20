import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rootItem

    required property var model
    readonly property var selectedNode: {
        model.treeRevision
        return model.nodeById(model.selectedNodeId)
    }

    function kindLabel(kind) {
        return ({ primitive: qsTr("Metric"), constant: qsTr("Constant"), add: "+", subtract: "−", multiply: "×", divide: "÷", runningSum: qsTr("RunningSum"), rollingMean: qsTr("RollingMean"), projectedFinalValue: qsTr("ProjectedFinalValue"), projectRateToFinal: qsTr("ProjectRateToFinal"), averageAcrossRuns: qsTr("AverageAcrossRuns") })[kind]
    }

    component RootChooser: RowLayout {
        ComboBox { id: chooser; model: rootItem.model.nodeKinds; Layout.fillWidth: true }
        Button { text: qsTr("Create expression"); onClicked: rootItem.model.replaceChild("", "root", chooser.currentText) }
    }

    Component {
        id: formulaComponent
        Flow {
            property var treeModel: rootItem.model
            property var node: ({ kind: "", selection: { kind: "recentRuns", count: 1, percent: 1 } })
            spacing: 3
            Button {
                visible: node.kind === "primitive" || node.kind === "constant"
                text: node.kind === "primitive" ? "⟨" + node.metric + "⟩" : "⟨" + node.value + "⟩"
                onClicked: treeModel.select(node.id)
            }
            Row {
                visible: treeModel.isBinary(node.kind)
                spacing: 3
                Button { text: "⟦"; onClicked: treeModel.select(node.id) }
                Loader { id: left; active: !!node.left; sourceComponent: formulaComponent; onLoaded: if (item && node.left) item.node = node.left }
                Label { text: rootItem.kindLabel(node.kind); font.bold: true }
                Loader { id: right; active: !!node.right; sourceComponent: formulaComponent; onLoaded: if (item && node.right) item.node = node.right }
                Button { text: "⟧"; onClicked: treeModel.select(node.id) }
            }
            Row {
                visible: !treeModel.isBinary(node.kind) && node.kind !== "primitive" && node.kind !== "constant"
                spacing: 3
                Button { text: rootItem.kindLabel(node.kind) + "("; onClicked: treeModel.select(node.id) }
                Loader { active: !!node.input; sourceComponent: formulaComponent; onLoaded: if (item && node.input) item.node = node.input }
                Button {
                    visible: node.kind === "rollingMean"
                    text: ", window: ⟨" + node.window + "⟩"
                    onClicked: treeModel.select(node.id)
                }
                Button {
                    visible: node.kind === "averageAcrossRuns"
                    text: node.kind === "averageAcrossRuns" && node.selection.kind === "recentRuns" ? "⦃recent " + node.selection.count + "⦄" : node.kind === "averageAcrossRuns" ? "⦃top " + node.selection.percent + "%⦄" : ""
                    onClicked: treeModel.select(node.id)
                }
                Label { text: ")" }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 170
            clip: true
            Flow {
                width: parent.width
                spacing: 4
                RootChooser { visible: !rootItem.model.root; width: parent.width }
                Repeater {
                    model: {
                        rootItem.model.treeRevision
                        return rootItem.model.root ? [rootItem.model.root] : []
                    }
                    delegate: Loader {
                        required property var modelData
                        sourceComponent: formulaComponent
                        onLoaded: {
                            item.treeModel = rootItem.model
                            item.node = modelData
                        }
                    }
                }
            }
        }
        GroupBox {
            title: qsTr("Selected expression")
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!rootItem.selectedNode
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                Label { text: rootItem.selectedNode ? rootItem.kindLabel(rootItem.selectedNode.kind) : ""; font.bold: true }
                ComboBox {
                    visible: rootItem.selectedNode && rootItem.selectedNode.kind === "primitive"
                    Layout.fillWidth: true
                    model: rootItem.model.primitiveMetrics
                    currentIndex: rootItem.selectedNode ? rootItem.model.primitiveMetrics.indexOf(rootItem.selectedNode.metric) : 0
                    onActivated: rootItem.model.updateField(rootItem.selectedNode.id, "metric", currentText)
                }
                SpinBox {
                    visible: rootItem.selectedNode && rootItem.selectedNode.kind === "constant"
                    Layout.fillWidth: true
                    from: -100000; to: 100000
                    value: rootItem.selectedNode && rootItem.selectedNode.kind === "constant" ? rootItem.selectedNode.value : 0
                    onValueModified: rootItem.model.updateField(rootItem.selectedNode.id, "value", value)
                }
                ComboBox {
                    visible: rootItem.selectedNode && rootItem.model.isBinary(rootItem.selectedNode.kind)
                    Layout.fillWidth: true
                    model: ["add", "subtract", "multiply", "divide"]
                    currentIndex: rootItem.selectedNode ? model.indexOf(rootItem.selectedNode.kind) : 0
                    onActivated: rootItem.model.changeBinaryOperator(rootItem.selectedNode.id, currentText)
                }
                SpinBox {
                    visible: rootItem.selectedNode && rootItem.selectedNode.kind === "rollingMean"
                    Layout.fillWidth: true
                    from: 1
                    to: 1000
                    value: rootItem.selectedNode ? rootItem.selectedNode.window : 1
                    onValueModified: rootItem.model.updateField(rootItem.selectedNode.id, "window", value)
                }
                RowLayout {
                    visible: rootItem.selectedNode && rootItem.selectedNode.kind === "averageAcrossRuns"
                    Layout.fillWidth: true
                    ComboBox {
                        id: selectionChooser
                        model: ["recentRuns", "topPercentile"]
                        currentIndex: rootItem.selectedNode && rootItem.selectedNode.kind === "averageAcrossRuns" && rootItem.selectedNode.selection.kind === "recentRuns" ? 0 : 1
                        displayText: currentText === "recentRuns" ? qsTr("Recent runs") : qsTr("Top percentile")
                        onActivated: rootItem.model.changeSelectionKind(rootItem.selectedNode.id, currentText)
                    }
                    SpinBox {
                        Layout.fillWidth: true
                        from: 1; to: 100
                        value: rootItem.selectedNode && rootItem.selectedNode.kind === "averageAcrossRuns" && rootItem.selectedNode.selection.kind === "recentRuns" ? rootItem.selectedNode.selection.count : rootItem.selectedNode && rootItem.selectedNode.kind === "averageAcrossRuns" ? rootItem.selectedNode.selection.percent : 1
                        onValueModified: rootItem.model.updateField(rootItem.selectedNode.id, rootItem.selectedNode.selection.kind === "recentRuns" ? "count" : "percent", value)
                    }
                }
                RowLayout {
                    visible: rootItem.selectedNode && rootItem.model.isBinary(rootItem.selectedNode.kind) && (!rootItem.selectedNode.left || !rootItem.selectedNode.right)
                    Layout.fillWidth: true
                    ComboBox { id: binaryChildKind; model: rootItem.model.nodeKinds; Layout.fillWidth: true }
                    Button {
                        visible: rootItem.selectedNode && !rootItem.selectedNode.left
                        text: qsTr("Add left")
                        onClicked: rootItem.model.replaceChild(rootItem.selectedNode.id, "left", binaryChildKind.currentText)
                    }
                    Button {
                        visible: rootItem.selectedNode && !rootItem.selectedNode.right
                        text: qsTr("Add right")
                        onClicked: rootItem.model.replaceChild(rootItem.selectedNode.id, "right", binaryChildKind.currentText)
                    }
                }
                RowLayout {
                    visible: rootItem.selectedNode && !rootItem.model.isBinary(rootItem.selectedNode.kind) && rootItem.selectedNode.kind !== "primitive" && rootItem.selectedNode.kind !== "constant" && !rootItem.selectedNode.input
                    Layout.fillWidth: true
                    ComboBox { id: unaryChildKind; model: rootItem.model.nodeKinds; Layout.fillWidth: true }
                    Button { text: qsTr("Add input"); onClicked: rootItem.model.replaceChild(rootItem.selectedNode.id, "input", unaryChildKind.currentText) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox { id: wrapper; model: ["runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns", "add", "subtract", "multiply", "divide"]; Layout.fillWidth: true }
                    Button { text: qsTr("Wrap selected"); onClicked: rootItem.model.wrapSelected(wrapper.currentText) }
                    Button { text: qsTr("Delete selected"); onClicked: rootItem.model.deleteNode(rootItem.selectedNode.id) }
                }
            }
        }
    }
}
