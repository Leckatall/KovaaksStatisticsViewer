import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rootItem

    required property var model
    property int selectionRevision: 0
    property var observedRoot: model ? model.root : {}

    implicitWidth: mainColumn.implicitWidth
    implicitHeight: mainColumn.implicitHeight

    function hasNode(node) { return !!node && node.kind !== undefined && node.kind !== "" }
    function kindLabel(kind) {
        return ({ primitive: qsTr("Metric"), constant: qsTr("Constant"), add: qsTr("Add"), subtract: qsTr("Subtract"),
                  multiply: qsTr("Multiply"), divide: qsTr("Divide"), runningSum: qsTr("Running sum"),
                  rollingMean: qsTr("Rolling mean"), projectedFinalValue: qsTr("Projected final value"),
                  projectRateToFinal: qsTr("Project rate to final"), averageAcrossRuns: qsTr("Average across runs") })[kind] || ""
    }
    function accentColor(kind, alpha) {
        const category = model.isBinary(kind) ? "binary" : kind === "averageAcrossRuns" ? "average" : (kind === "primitive" || kind === "constant") ? "leaf" : "unary"
        const rgb = ({ leaf: [76, 175, 80], binary: [255, 152, 0], average: [156, 39, 176], unary: [33, 150, 243] })[category]
        return Qt.rgba(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, alpha)
    }
    function ensureSelection() {
        if (hasNode(model.root) && !model.selectedNodeId) {
            model.select(model.root.id)
            selectionRevision++
        }
    }

    Component.onCompleted: Qt.callLater(ensureSelection)
    onModelChanged: Qt.callLater(ensureSelection)
    onObservedRootChanged: Qt.callLater(ensureSelection)
    Timer { interval: 1; running: true; repeat: false; onTriggered: rootItem.ensureSelection() }

    component KindChooser: RowLayout {
        property string parentId: ""
        property string slot: "root"
        Label { text: qsTr("Add:") }
        ComboBox {
            id: chooser
            objectName: "kindChooser_" + parentId + "_" + slot
            Layout.fillWidth: true
            model: rootItem.model.nodeKinds
            displayText: rootItem.kindLabel(currentText)
            delegate: ItemDelegate { required property var modelData; width: chooser.width; text: rootItem.kindLabel(modelData) }
        }
        Button {
            objectName: "addNodeButton_" + parentId + "_" + slot
            text: qsTr("Add")
            onClicked: rootItem.model.replaceChild(parentId, slot, chooser.currentText)
        }
    }

    component FoldedChip: Rectangle {
        required property var childNode
        required property var treeModel
        implicitHeight: content.implicitHeight + 12
        radius: 6
        color: rootItem.accentColor(childNode.kind, 0.12)
        border.color: rootItem.accentColor(childNode.kind, 0.45)
        RowLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 6
            Label { Layout.fillWidth: true; text: treeModel.describe(childNode); wrapMode: Text.WordWrap }
            Label { text: "›"; opacity: 0.6 }
        }
        MouseArea { anchors.fill: parent; onClicked: treeModel.select(childNode.id) }
    }

    Component {
        id: primitiveEditor
        ComboBox {
            property var node
            property var treeModel
            objectName: "metricComboBox_" + node.id
            Layout.fillWidth: true
            model: treeModel.primitiveMetrics
            currentIndex: treeModel.primitiveMetrics.indexOf(node.metric)
            onActivated: treeModel.updateField(node.id, "metric", currentText)
        }
    }
    Component {
        id: constantEditor
        SpinBox {
            property var node
            property var treeModel
            objectName: "constantSpinBox_" + node.id
            Layout.fillWidth: true
            from: -100000
            to: 100000
            value: { treeModel.treeRevision; return Number.isFinite(node.value) ? node.value : 0 }
            onValueModified: treeModel.updateField(node.id, "value", value)
        }
    }
    Component {
        id: binaryEditor
        ComboBox {
            property var node
            property var treeModel
            objectName: "operatorComboBox_" + node.id
            Layout.fillWidth: true
            model: ["add", "subtract", "multiply", "divide"]
            displayText: rootItem.kindLabel(currentText)
            delegate: ItemDelegate { required property var modelData; text: rootItem.kindLabel(modelData) }
            onActivated: treeModel.changeBinaryOperator(node.id, currentText)
            Binding on currentIndex { value: { treeModel.treeRevision; return model.indexOf(node.kind) } }
        }
    }
    Component {
        id: rollingMeanEditor
        SpinBox {
            property var node
            property var treeModel
            objectName: "windowSpinBox_" + node.id
            Layout.fillWidth: true
            from: 1
            to: 1000
            value: { treeModel.treeRevision; return Number.isFinite(node.window) ? node.window : 1 }
            onValueModified: treeModel.updateField(node.id, "window", value)
        }
    }
    Component {
        id: averageEditor
        RowLayout {
            property var node
            property var treeModel
            ComboBox {
                id: selectionKind
                objectName: "selectionKindComboBox_" + parent.node.id
                model: ["recentRuns", "topPercentile"]
                currentIndex: { parent.treeModel.treeRevision; return parent.node.selection.kind === "recentRuns" ? 0 : 1 }
                displayText: currentText === "recentRuns" ? qsTr("Recent runs") : qsTr("Top percentile")
                onActivated: parent.treeModel.changeSelectionKind(parent.node.id, currentText)
            }
            SpinBox {
                objectName: "selectionValueSpinBox_" + parent.node.id
                Layout.fillWidth: true
                from: 1
                to: 100
                value: { parent.treeModel.treeRevision; return parent.node.selection.kind === "recentRuns" ? parent.node.selection.count : parent.node.selection.percent }
                onValueModified: parent.treeModel.updateField(parent.node.id, parent.node.selection.kind === "recentRuns" ? "count" : "percent", value)
            }
        }
    }

    Component {
        id: pathCardComponent
        Rectangle {
            id: shell
            property var treeModel: rootItem.model
            property var chain: []
            property int index: 0
            readonly property var node: chain[index] || {}
            readonly property bool isFocused: index >= chain.length - 1
            objectName: "pathCard_" + shell.node.id
            implicitWidth: cardColumn.implicitWidth + cardColumn.anchors.margins * 2
            implicitHeight: cardColumn.implicitHeight + cardColumn.anchors.margins * 2
            color: rootItem.accentColor(node.kind, 0.12)
            border.color: rootItem.accentColor(node.kind, 0.45)
            border.width: 1
            radius: 6
            ColumnLayout {
                id: cardColumn
                anchors.fill: parent
                anchors.margins: 7
                spacing: 6
                Item {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: headerRow.implicitHeight
                    MouseArea { anchors.fill: parent; onClicked: shell.treeModel.select(shell.node.id) }
                    RowLayout {
                        id: headerRow
                        anchors.fill: parent
                        Label { text: rootItem.kindLabel(shell.node.kind); font.bold: true; color: rootItem.accentColor(shell.node.kind, 1) }
                        Item { Layout.fillWidth: true }
                        Button { objectName: "deleteNodeButton_" + shell.node.id; text: qsTr("Delete"); onClicked: shell.treeModel.deleteNode(shell.node.id) }
                    }
                }
                ColumnLayout {
                    visible: shell.isFocused
                    Layout.fillWidth: true
                    Item {
                        id: fieldLoader
                        Layout.fillWidth: true
                        implicitHeight: item ? item.implicitHeight : 0
                        property var node: shell.node
                        property var treeModel: shell.treeModel
                        property int revision: treeModel.treeRevision
                        property var item: null
                        readonly property var editors: ({ primitive: primitiveEditor, constant: constantEditor, add: binaryEditor, subtract: binaryEditor, multiply: binaryEditor, divide: binaryEditor, rollingMean: rollingMeanEditor, averageAcrossRuns: averageEditor })
                        readonly property var currentComponent: shell.isFocused ? (editors[node.kind] ?? null) : null
                        function bindLoadedItem() {
                            item.node = ({})
                            item.node = Qt.binding(function() { return fieldLoader.node })
                            item.treeModel = Qt.binding(function() { return fieldLoader.treeModel })
                        }
                        function rebuild() {
                            if (item) {
                                item.destroy()
                                item = null
                            }
                            if (currentComponent) {
                                item = currentComponent.createObject(fieldLoader, {
                                    node: Qt.binding(function() { return fieldLoader.node }),
                                    treeModel: Qt.binding(function() { return fieldLoader.treeModel })
                                })
                            }
                        }
                        onCurrentComponentChanged: rebuild()
                        onRevisionChanged: if (item) bindLoadedItem()
                        Component.onCompleted: rebuild()
                    }
                    Repeater {
                        model: shell.treeModel.childSlotsFor(shell.node.kind)
                        delegate: ColumnLayout {
                            required property string modelData
                            Layout.fillWidth: true
                            readonly property var childNode: shell.node[modelData]
                            FoldedChip {
                                visible: rootItem.hasNode(parent.childNode)
                                Layout.fillWidth: true
                                treeModel: shell.treeModel
                                childNode: parent.childNode
                            }
                            KindChooser {
                                visible: !rootItem.hasNode(parent.childNode)
                                Layout.fillWidth: true
                                parentId: shell.node.id
                                slot: parent.modelData
                            }
                        }
                    }
                }
                Loader {
                    id: nestedLoader
                    active: !shell.isFocused
                    visible: !shell.isFocused
                    Layout.fillWidth: true
                    sourceComponent: pathCardComponent
                    property var chain: shell.chain
                    property int childIndex: shell.index + 1
                    onChainChanged: if (item) item.chain = chain
                    onChildIndexChanged: if (item) item.index = childIndex
                    onLoaded: { item.treeModel = shell.treeModel; item.chain = chain; item.index = childIndex }
                }
            }
        }
    }

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        spacing: 8
        KindChooser { visible: !rootItem.hasNode(rootItem.model.root); Layout.fillWidth: true }
        ScrollView {
            id: expressionTreeScrollView
            objectName: "expressionTreeScrollView"
            visible: rootItem.hasNode(rootItem.model.root)
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            // Keep the unclamped content request visible to the outer dialog.
            implicitWidth: rootLoader.implicitWidth
            implicitHeight: rootLoader.implicitHeight

            Loader {
                id: rootLoader
                active: expressionTreeScrollView.visible
                width: expressionTreeScrollView.availableWidth
                sourceComponent: pathCardComponent
                property var chain: { rootItem.model.treeRevision; rootItem.selectionRevision; return rootItem.model.ancestorChain(rootItem.model.selectedNodeId) }
                onChainChanged: if (item) item.chain = chain
                onLoaded: { item.treeModel = rootItem.model; item.chain = chain; item.index = 0 }
            }
        }
        RowLayout {
            visible: !!rootItem.model.selectedNodeId
            Layout.fillWidth: true
            Label { text: qsTr("Wrap:") }
            ComboBox {
                id: wrapperKind
                objectName: "wrapKindComboBox"
                Layout.fillWidth: true
                model: ["runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns", "add", "subtract", "multiply", "divide"]
                displayText: rootItem.kindLabel(currentText)
            }
            Button { objectName: "wrapNodeButton"; text: qsTr("Wrap"); onClicked: rootItem.model.wrapSelected(wrapperKind.currentText) }
        }
    }
}
