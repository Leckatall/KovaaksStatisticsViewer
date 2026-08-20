import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rootItem

    required property var model

    function kindLabel(kind) {
        return ({
            primitive: qsTr("Metric"), constant: qsTr("Constant"), add: qsTr("Add"), subtract: qsTr("Subtract"),
            multiply: qsTr("Multiply"), divide: qsTr("Divide"), runningSum: qsTr("Running sum"),
            rollingMean: qsTr("Rolling mean"), projectedFinalValue: qsTr("Projected final value"),
            projectRateToFinal: qsTr("Project rate to final"), averageAcrossRuns: qsTr("Average across runs")
        })[kind] || ""
    }

    readonly property var accentRgb: ({
        leaf: [76, 175, 80], binary: [255, 152, 0],
        averageAcrossRuns: [156, 39, 176], unary: [33, 150, 243]
    })

    function accentCategory(kind) {
        if (kind === "primitive" || kind === "constant") return "leaf"
        if (model.isBinary(kind)) return "binary"
        if (kind === "averageAcrossRuns") return "averageAcrossRuns"
        return "unary"
    }

    function accentColor(kind, alpha) {
        const rgb = accentRgb[accentCategory(kind)]
        return Qt.rgba(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, alpha)
    }

    component KindChooser: RowLayout {
        property string parentId: ""
        property string slot: "root"
        spacing: 6
        Label { text: qsTr("Add:") }
        ComboBox {
            id: chooser
            model: rootItem.model.nodeKinds
            textRole: ""
            Layout.fillWidth: true
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: chooser.width
                text: rootItem.kindLabel(modelData)
            }
            displayText: rootItem.kindLabel(currentText)
        }
        Button {
            text: qsTr("Add")
            onClicked: rootItem.model.replaceChild(parentId, slot, chooser.currentText)
        }
    }

    component FoldedChip: Rectangle {
        id: chip
        required property var treeModel
        required property var childNode
        implicitHeight: chipContent.implicitHeight + 12
        radius: 6
        color: rootItem.accentColor(chip.childNode ? chip.childNode.kind : "", 0.12)
        border.color: rootItem.accentColor(chip.childNode ? chip.childNode.kind : "", 0.45)
        border.width: 1

        RowLayout {
            id: chipContent
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: chip.childNode ? chip.treeModel.describe(chip.childNode) : ""
            }
            Label { text: "›"; opacity: 0.6 }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: if (chip.childNode) chip.treeModel.select(chip.childNode.id)
        }
    }

    // Recursive: one Rectangle per level of the ancestor chain (root..selected). Every level but
    // the deepest collapses to just its header and nests the next level in the remaining space;
    // the deepest level is the only one that shows editable fields and its own child slots.
    Component {
        id: pathCardComponent
        Rectangle {
            id: shell
            property var treeModel: rootItem.model
            property var chain: []
            property int index: 0
            readonly property var node: chain[index] || { kind: "", selection: { kind: "recentRuns", count: 1, percent: 1 } }
            readonly property bool isFocused: index >= chain.length - 1
            color: rootItem.accentColor(shell.node.kind, 0.12)
            border.color: rootItem.accentColor(shell.node.kind, 0.45)
            border.width: 1
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 7
                spacing: 6

                Item {
                    id: header
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: headerRow.implicitHeight
                    MouseArea {
                        anchors.fill: parent
                        onClicked: shell.treeModel.select(shell.node.id)
                    }
                    RowLayout {
                        id: headerRow
                        anchors.fill: parent
                        Label { text: rootItem.kindLabel(shell.node.kind); font.bold: true; color: rootItem.accentColor(shell.node.kind, 1.0) }
                        Item { Layout.fillWidth: true }
                        Button { text: qsTr("Delete"); onClicked: shell.treeModel.deleteNode(shell.node.id) }
                    }
                }

                ColumnLayout {
                    visible: shell.isFocused
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6

                    ComboBox {
                        visible: shell.node.kind === "primitive"
                        Layout.fillWidth: true
                        model: shell.treeModel.primitiveMetrics
                        currentIndex: shell.treeModel.primitiveMetrics.indexOf(shell.node.metric)
                        onActivated: shell.treeModel.updateField(shell.node.id, "metric", currentText)
                    }
                    SpinBox {
                        visible: shell.node.kind === "constant"
                        Layout.fillWidth: true
                        from: -100000
                        to: 100000
                        value: Number.isFinite(shell.node.value) ? shell.node.value : 0
                        onValueModified: shell.treeModel.updateField(shell.node.id, "value", value)
                    }
                    ComboBox {
                        visible: shell.treeModel.isBinary(shell.node.kind)
                        Layout.fillWidth: true
                        model: ["add", "subtract", "multiply", "divide"]
                        currentIndex: model.indexOf(shell.node.kind)
                        textRole: ""
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            width: parent.width
                            text: rootItem.kindLabel(modelData)
                        }
                        displayText: rootItem.kindLabel(currentText)
                        onActivated: shell.treeModel.changeBinaryOperator(shell.node.id, currentText)
                    }
                    SpinBox {
                        visible: shell.node.kind === "rollingMean"
                        Layout.fillWidth: true
                        from: 1
                        to: 1000
                        value: Number.isFinite(shell.node.window) ? shell.node.window : 1
                        onValueModified: shell.treeModel.updateField(shell.node.id, "window", value)
                    }
                    RowLayout {
                        visible: shell.node.kind === "averageAcrossRuns"
                        Layout.fillWidth: true
                        ComboBox {
                            id: selectionKind
                            model: ["recentRuns", "topPercentile"]
                            currentIndex: shell.node.kind === "averageAcrossRuns" && shell.node.selection.kind === "recentRuns" ? 0 : 1
                            textRole: ""
                            displayText: currentText === "recentRuns" ? qsTr("Recent runs") : qsTr("Top percentile")
                            onActivated: shell.treeModel.changeSelectionKind(shell.node.id, currentText)
                        }
                        SpinBox {
                            Layout.fillWidth: true
                            from: 1
                            to: 100
                            value: shell.node.kind === "averageAcrossRuns" && shell.node.selection.kind === "recentRuns" ? shell.node.selection.count : shell.node.kind === "averageAcrossRuns" ? shell.node.selection.percent : 1
                            onValueModified: shell.treeModel.updateField(shell.node.id, shell.node.kind === "averageAcrossRuns" && shell.node.selection.kind === "recentRuns" ? "count" : "percent", value)
                        }
                    }

                    FoldedChip {
                        visible: { shell.treeModel.treeRevision; return shell.treeModel.isBinary(shell.node.kind) && !!shell.node.left }
                        Layout.fillWidth: true
                        treeModel: shell.treeModel
                        childNode: { shell.treeModel.treeRevision; return shell.node.left }
                    }
                    KindChooser {
                        visible: { shell.treeModel.treeRevision; return shell.treeModel.isBinary(shell.node.kind) && !shell.node.left }
                        Layout.fillWidth: true
                        parentId: shell.node.id || ""
                        slot: "left"
                    }
                    FoldedChip {
                        visible: { shell.treeModel.treeRevision; return shell.treeModel.isBinary(shell.node.kind) && !!shell.node.right }
                        Layout.fillWidth: true
                        treeModel: shell.treeModel
                        childNode: { shell.treeModel.treeRevision; return shell.node.right }
                    }
                    KindChooser {
                        visible: { shell.treeModel.treeRevision; return shell.treeModel.isBinary(shell.node.kind) && !shell.node.right }
                        Layout.fillWidth: true
                        parentId: shell.node.id || ""
                        slot: "right"
                    }
                    FoldedChip {
                        visible: {
                            shell.treeModel.treeRevision
                            return !shell.treeModel.isBinary(shell.node.kind) && shell.node.kind !== "primitive" && shell.node.kind !== "constant" && shell.node.kind !== "" && !!shell.node.input
                        }
                        Layout.fillWidth: true
                        treeModel: shell.treeModel
                        childNode: { shell.treeModel.treeRevision; return shell.node.input }
                    }
                    KindChooser {
                        visible: {
                            shell.treeModel.treeRevision
                            return !shell.treeModel.isBinary(shell.node.kind) && shell.node.kind !== "primitive" && shell.node.kind !== "constant" && shell.node.kind !== "" && !shell.node.input
                        }
                        Layout.fillWidth: true
                        parentId: shell.node.id || ""
                        slot: "input"
                    }
                }

                Loader {
                    id: nestedLoader
                    visible: !shell.isFocused
                    active: !shell.isFocused
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: pathCardComponent
                    property var chain: shell.chain
                    property int childIndex: shell.index + 1
                    onChainChanged: if (item) item.chain = chain
                    onChildIndexChanged: if (item) item.index = childIndex
                    onLoaded: {
                        item.treeModel = shell.treeModel
                        item.chain = chain
                        item.index = childIndex
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            KindChooser {
                visible: !rootItem.model.root
                Layout.fillWidth: true
                parentId: ""
                slot: "root"
            }

            Loader {
                id: rootLoader
                visible: !!rootItem.model.root
                active: !!rootItem.model.root
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: pathCardComponent
                property var chain: {
                    rootItem.model.treeRevision
                    return rootItem.model.ancestorChain(rootItem.model.selectedNodeId)
                }
                onChainChanged: if (item) item.chain = chain
                onLoaded: {
                    item.treeModel = rootItem.model
                    item.chain = chain
                    item.index = 0
                }
            }
        }

        RowLayout {
            visible: !!rootItem.model.selectedNodeId
            Layout.fillWidth: true
            Label { text: qsTr("Wrap:") }
            ComboBox {
                id: wrapperKind
                Layout.fillWidth: true
                model: ["runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns", "add", "subtract", "multiply", "divide"]
                textRole: ""
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: parent.width
                    text: rootItem.kindLabel(modelData)
                }
                displayText: rootItem.kindLabel(currentText)
            }
            Button { text: qsTr("Wrap"); onClicked: rootItem.model.wrapSelected(wrapperKind.currentText) }
        }
    }
}
