import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rootItem

    required property var model
    property var observedRoot: model ? model.root : {}
    property int selectionRevision: 0

    function accentColor(kind, alpha) {
        const category = model.isBinary(kind) ? "binary" : kind === "averageAcrossRuns" ? "average" : (kind === "primitive" || kind === "constant") ? "leaf" : "unary";
        const rgb = ({
                leaf: [76, 175, 80],
                binary: [255, 152, 0],
                average: [156, 39, 176],
                unary: [33, 150, 243]
            })[category];
        return Qt.rgba(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, alpha);
    }
    function ensureSelection() {
        if (hasNode(model.root) && !model.selectedNodeId) {
            model.select(model.root.id);
            selectionRevision++;
        }
    }
    function hasNode(node) {
        return !!node && node.kind !== undefined && node.kind !== "";
    }
    function kindLabel(kind) {
        return ({
                primitive: qsTr("Metric"),
                constant: qsTr("Constant"),
                add: qsTr("Add"),
                subtract: qsTr("Subtract"),
                multiply: qsTr("Multiply"),
                divide: qsTr("Divide"),
                runningSum: qsTr("Running sum"),
                rollingMean: qsTr("Rolling mean"),
                projectedFinalValue: qsTr("Projected final value"),
                projectRateToFinal: qsTr("Project rate to final"),
                averageAcrossRuns: qsTr("Average across runs")
            })[kind] || "";
    }

    implicitHeight: mainColumn.implicitHeight
    implicitWidth: mainColumn.implicitWidth

    Component.onCompleted: Qt.callLater(ensureSelection)
    onModelChanged: Qt.callLater(ensureSelection)
    onObservedRootChanged: Qt.callLater(ensureSelection)

    Timer {
        interval: 1
        repeat: false
        running: true

        onTriggered: rootItem.ensureSelection()
    }
    Component {
        id: primitiveEditor

        ComboBox {
            required property var node
            required property var treeModel

            Layout.fillWidth: true
            currentIndex: treeModel.primitiveMetrics.indexOf(node.metric)
            model: treeModel.primitiveMetrics
            objectName: "metricComboBox_" + node.id

            onActivated: treeModel.updateField(node.id, "metric", currentText)
        }
    }
    Component {
        id: constantEditor

        SpinBox {
            required property var node
            required property var treeModel

            Layout.fillWidth: true
            from: -100000
            objectName: "constantSpinBox_" + node.id
            to: 100000
            value: {
                treeModel.treeRevision;
                return Number.isFinite(node.value) ? node.value : 0;
            }

            onValueModified: treeModel.updateField(node.id, "value", value)
        }
    }
    Component {
        id: binaryEditor

        ComboBox {
            required property var node
            required property var treeModel

            Layout.fillWidth: true
            displayText: rootItem.kindLabel(currentText)
            model: ["add", "subtract", "multiply", "divide"]
            objectName: "operatorComboBox_" + node.id

            Binding on currentIndex {
                value: {
                    treeModel.treeRevision;
                    return model.indexOf(node.kind);
                }
            }
            delegate: ItemDelegate {
                required property var modelData

                text: rootItem.kindLabel(modelData)
            }

            onActivated: treeModel.changeBinaryOperator(node.id, currentText)
        }
    }
    Component {
        id: rollingMeanEditor

        SpinBox {
            required property var node
            required property var treeModel

            Layout.fillWidth: true
            from: 1
            objectName: "windowSpinBox_" + node.id
            to: 1000
            value: {
                treeModel.treeRevision;
                return Number.isFinite(node.window) ? node.window : 1;
            }

            onValueModified: treeModel.updateField(node.id, "window", value)
        }
    }
    Component {
        id: averageEditor

        RowLayout {
            id: averageInstance

            required property var node
            required property var treeModel

            ComboBox {
                id: selectionKind

                currentIndex: {
                    averageInstance.treeModel.treeRevision;
                    return node.selection ? (averageInstance.node.selection.kind === "recentRuns" ? 0 : 1) : 0;
                }
                displayText: currentText === "recentRuns" ? qsTr("Recent runs") : qsTr("Top percentile")
                model: ["recentRuns", "topPercentile"]
                objectName: "selectionKindComboBox_" + averageInstance.node.id

                onActivated: averageInstance.treeModel.changeSelectionKind(averageInstance.node.id, currentText)
            }
            SpinBox {
                Layout.fillWidth: true
                from: 1
                objectName: "selectionValueSpinBox_" + averageInstance.node.id
                to: 100
                value: {
                    averageInstance.treeModel.treeRevision;
                    return node.selection ? (averageInstance.node.selection.kind === "recentRuns" ? averageInstance.node.selection.count : averageInstance.node.selection.percent) : 1;
                }

                onValueModified: averageInstance.treeModel.updateField(averageInstance.node.id, averageInstance.node.selection.kind === "recentRuns" ? "count" : "percent", value)
            }
        }
    }
    Component {
        id: pathCardComponent

        Rectangle {
            id: shell

            property var chain: []
            property int index: 0
            readonly property bool isFocused: index >= chain.length - 1
            readonly property var node: chain[index] || {}
            property var treeModel: rootItem.model

            border.color: rootItem.accentColor(node.kind, 0.45)
            border.width: 1
            color: rootItem.accentColor(node.kind, 0.12)
            implicitHeight: cardColumn.implicitHeight + cardColumn.anchors.margins * 2
            implicitWidth: cardColumn.implicitWidth + cardColumn.anchors.margins * 2
            objectName: "pathCard_" + shell.node.id
            radius: 6

            ColumnLayout {
                id: cardColumn

                anchors.fill: parent
                anchors.margins: 7
                spacing: 6

                Item {
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    implicitHeight: headerRow.implicitHeight

                    MouseArea {
                        anchors.fill: parent

                        onClicked: shell.treeModel.select(shell.node.id)
                    }
                    RowLayout {
                        id: headerRow

                        anchors.fill: parent

                        Label {
                            color: rootItem.accentColor(shell.node.kind, 1)
                            font.bold: true
                            text: rootItem.kindLabel(shell.node.kind)
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Button {
                            objectName: "deleteNodeButton_" + shell.node.id
                            text: qsTr("Delete")

                            onClicked: shell.treeModel.deleteNode(shell.node.id)
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: shell.isFocused

                    Item {
                        id: fieldLoader

                        readonly property var currentComponent: shell.isFocused ? (editors[node.kind] ?? null) : null
                        readonly property var editors: ({
                                primitive: primitiveEditor,
                                constant: constantEditor,
                                add: binaryEditor,
                                subtract: binaryEditor,
                                multiply: binaryEditor,
                                divide: binaryEditor,
                                rollingMean: rollingMeanEditor,
                                averageAcrossRuns: averageEditor
                            })
                        property var item: null
                        property var node: shell.node
                        property int revision: treeModel.treeRevision
                        property var treeModel: shell.treeModel

                        function bindLoadedItem() {
                            item.node = ({});
                            item.node = Qt.binding(function () {
                                return fieldLoader.node;
                            });
                            item.treeModel = Qt.binding(function () {
                                return fieldLoader.treeModel;
                            });
                        }
                        function rebuild() {
                            if (item) {
                                item.destroy();
                                item = null;
                            }
                            if (currentComponent) {
                                item = currentComponent.createObject(fieldLoader, {
                                    node: Qt.binding(function () {
                                        return fieldLoader.node;
                                    }),
                                    treeModel: Qt.binding(function () {
                                        return fieldLoader.treeModel;
                                    })
                                });
                            }
                        }

                        Layout.fillWidth: true
                        implicitHeight: item ? item.implicitHeight : 0

                        Component.onCompleted: rebuild()
                        onCurrentComponentChanged: rebuild()
                        onRevisionChanged: if (item)
                            bindLoadedItem()
                    }
                    Repeater {
                        model: shell.treeModel.childSlotsFor(shell.node.kind)

                        delegate: ColumnLayout {
                            readonly property var childNode: shell.node[modelData]
                            required property string modelData

                            Layout.fillWidth: true

                            FoldedChip {
                                Layout.fillWidth: true
                                childNode: parent.childNode
                                treeModel: shell.treeModel
                                visible: rootItem.hasNode(parent.childNode)
                            }
                            KindChooser {
                                Layout.fillWidth: true
                                parentId: shell.node.id
                                slot: parent.modelData
                                visible: !rootItem.hasNode(parent.childNode)
                            }
                        }
                    }
                }
                Loader {
                    id: nestedLoader

                    property var chain: shell.chain
                    property int childIndex: shell.index + 1

                    Layout.fillWidth: true
                    active: !shell.isFocused
                    sourceComponent: pathCardComponent
                    visible: !shell.isFocused

                    onChainChanged: if (item)
                        item.chain = chain
                    onChildIndexChanged: if (item)
                        item.index = childIndex
                    onLoaded: {
                        item.treeModel = shell.treeModel;
                        item.chain = chain;
                        item.index = childIndex;
                    }
                }
            }
        }
    }
    ColumnLayout {
        id: mainColumn

        anchors.fill: parent
        spacing: 8

        KindChooser {
            Layout.fillWidth: true
            visible: !rootItem.hasNode(rootItem.model.root)
        }
        ScrollView {
            id: expressionTreeScrollView

            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            contentWidth: availableWidth
            implicitHeight: rootLoader.implicitHeight
            // Keep the unclamped content request visible to the outer dialog.
            implicitWidth: rootLoader.implicitWidth
            objectName: "expressionTreeScrollView"
            visible: rootItem.hasNode(rootItem.model.root)

            Loader {
                id: rootLoader

                property var chain: {
                    rootItem.model.treeRevision;
                    rootItem.selectionRevision;
                    return rootItem.model.ancestorChain(rootItem.model.selectedNodeId);
                }

                active: expressionTreeScrollView.visible
                sourceComponent: pathCardComponent
                width: expressionTreeScrollView.availableWidth

                onChainChanged: if (item)
                    item.chain = chain
                onLoaded: {
                    item.treeModel = rootItem.model;
                    item.chain = chain;
                    item.index = 0;
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !!rootItem.model.selectedNodeId

            Label {
                text: qsTr("Wrap:")
            }
            ComboBox {
                id: wrapperKind

                Layout.fillWidth: true
                displayText: rootItem.kindLabel(currentText)
                model: ["runningSum", "rollingMean", "projectedFinalValue", "projectRateToFinal", "averageAcrossRuns", "add", "subtract", "multiply", "divide"]
                objectName: "wrapKindComboBox"
            }
            Button {
                objectName: "wrapNodeButton"
                text: qsTr("Wrap")

                onClicked: rootItem.model.wrapSelected(wrapperKind.currentText)
            }
        }
    }

    component FoldedChip: Rectangle {
        required property var childNode
        required property var treeModel

        border.color: rootItem.accentColor(childNode.kind, 0.45)
        color: rootItem.accentColor(childNode.kind, 0.12)
        implicitHeight: content.implicitHeight + 12
        radius: 6

        RowLayout {
            id: content

            anchors.fill: parent
            anchors.margins: 6

            Label {
                Layout.fillWidth: true
                text: treeModel.describe(childNode)
                wrapMode: Text.WordWrap
            }
            Label {
                opacity: 0.6
                text: "›"
            }
        }
        MouseArea {
            anchors.fill: parent

            onClicked: treeModel.select(childNode.id)
        }
    }
    component KindChooser: RowLayout {
        property string parentId: ""
        property string slot: "root"

        Label {
            text: qsTr("Add:")
        }
        ComboBox {
            id: chooser

            Layout.fillWidth: true
            displayText: rootItem.kindLabel(currentText)
            model: rootItem.model.nodeKinds
            objectName: "kindChooser_" + parentId + "_" + slot

            delegate: ItemDelegate {
                required property var modelData

                text: rootItem.kindLabel(modelData)
                width: chooser.width
            }
        }
        Button {
            objectName: "addNodeButton_" + parentId + "_" + slot
            text: qsTr("Add")

            onClicked: rootItem.model.replaceChild(parentId, slot, chooser.currentText)
        }
    }
}
