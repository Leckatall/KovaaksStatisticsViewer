import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: rootItem

    required property var model

    readonly property var ancestorChain: rootItem.model && rootItem.model.selected ? rootItem.model.ancestorChain(rootItem.model.selected) : []

    function accentColor(kind, alpha) {
        const category = model.isBinary(kind) ? "binary" : kind === "averageAcrossRuns" ? "average" : (kind === "primitive" || kind === "constant") ? "leaf" : "unary";
        const rgb = ({ leaf: [76, 175, 80], binary: [255, 152, 0], average: [156, 39, 176], unary: [33, 150, 243] })[category];
        return Qt.rgba(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, alpha);
    }
    function kindLabel(kind) {
        return ({ primitive: qsTr("Metric"), constant: qsTr("Constant"), add: qsTr("Add"), subtract: qsTr("Subtract"), multiply: qsTr("Multiply"), divide: qsTr("Divide"), runningSum: qsTr("Running sum"), rollingMean: qsTr("Rolling mean"), projectedFinalValue: qsTr("Projected final value"), projectRateToFinal: qsTr("Project rate to final"), averageAcrossRuns: qsTr("Average across runs") })[kind] || "";
    }

    implicitHeight: mainColumn.implicitHeight
    implicitWidth: mainColumn.implicitWidth

    function selectRootIfUnselected() {
        if (rootItem.model && rootItem.model.root && !rootItem.model.selected)
            rootItem.model.select(rootItem.model.root);
    }

    Component.onCompleted: rootItem.selectRootIfUnselected()
    onModelChanged: rootItem.selectRootIfUnselected()

    // The tree renders off the selected node's ancestor chain, and loadFrom (e.g. Paste) clears the
    // selection while swapping the root — without this the editor would show nothing until reopened.
    Connections {
        target: rootItem.model
        function onRootChanged() { rootItem.selectRootIfUnselected() }
    }

    ColumnLayout {
        id: mainColumn

        anchors.fill: parent
        spacing: 8

        KindChooser {
            Layout.fillWidth: true
            kindLabel: rootItem.kindLabel
            parentNode: null
            slot: "root"
            treeModel: rootItem.model
            visible: !!rootItem.model && !rootItem.model.root
        }
        ScrollView {
            id: expressionTreeScrollView

            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            contentWidth: availableWidth
            implicitHeight: breadcrumbColumn.implicitHeight
            implicitWidth: 400
            objectName: "expressionTreeScrollView"
            visible: !!rootItem.model && !!rootItem.model.root

            ColumnLayout {
                id: breadcrumbColumn

                spacing: 6
                width: expressionTreeScrollView.availableWidth

                Repeater {
                    model: rootItem.ancestorChain

                    delegate: PathCard {
                        id: pathCard

                        required property int index
                        required property var modelData

                        focused: index === rootItem.ancestorChain.length - 1

                        property Component primitiveEditorComponent: Component {
                            FieldEditors.PrimitiveEditor {
                                node: pathCard.node
                                treeModel: rootItem.model
                            }
                        }
                        property Component constantEditorComponent: Component { FieldEditors.ConstantEditor { node: pathCard.node } }
                        property Component binaryOpEditorComponent: Component {
                            FieldEditors.BinaryOpEditor {
                                node: pathCard.node
                                kindLabel: rootItem.kindLabel
                            }
                        }
                        property Component rollingMeanEditorComponent: Component { FieldEditors.RollingMeanEditor { node: pathCard.node } }
                        property Component averageAcrossRunsEditorComponent: Component { FieldEditors.AverageAcrossRunsEditor { node: pathCard.node } }

                        function editorComponentFor(kind) {
                            return ({ primitive: primitiveEditorComponent, constant: constantEditorComponent, add: binaryOpEditorComponent, subtract: binaryOpEditorComponent, multiply: binaryOpEditorComponent, divide: binaryOpEditorComponent, rollingMean: rollingMeanEditorComponent, averageAcrossRuns: averageAcrossRunsEditorComponent })[kind] || null;
                        }

                        Layout.fillWidth: true
                        Layout.leftMargin: index * 16
                        accentColorFn: rootItem.accentColor
                        kindLabel: rootItem.kindLabel
                        node: modelData
                        objectName: "pathCard_" + index

                        onDeleteRequested: rootItem.model.deleteNode(pathCard.node)
                        onSelected: rootItem.model.select(pathCard.node)

                        Loader {
                            Layout.fillWidth: true
                            active: pathCard.focused
                            sourceComponent: pathCard.focused && pathCard.node ? pathCard.editorComponentFor(pathCard.node.kind) : null
                            visible: pathCard.focused
                        }
                        Repeater {
                            model: pathCard.focused && rootItem.model && pathCard.node ? rootItem.model.childSlotsFor(pathCard.node.kind) : []

                            delegate: ColumnLayout {
                                id: slotRow

                                required property string modelData
                                readonly property var childNode: pathCard.node[slotRow.modelData]

                                Layout.fillWidth: true

                                Loader {
                                    Layout.fillWidth: true
                                    active: !!slotRow.childNode
                                    sourceComponent: Component {
                                        FoldedChip {
                                            accentColorFn: rootItem.accentColor
                                            childNode: slotRow.childNode
                                            objectName: "foldedChip_" + pathCard.index + "_" + slotRow.modelData
                                            treeModel: rootItem.model

                                            onSelected: if (rootItem.model && slotRow.childNode) rootItem.model.select(slotRow.childNode)
                                        }
                                    }
                                }
                                KindChooser {
                                    Layout.fillWidth: true
                                    kindLabel: rootItem.kindLabel
                                    parentNode: pathCard.node
                                    slot: slotRow.modelData
                                    treeModel: rootItem.model
                                    visible: !!rootItem.model && !slotRow.childNode
                                }
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: !!rootItem.model && !!rootItem.model.selected

            Label { text: qsTr("Wrap:") }
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
        RowLayout {
            Layout.fillWidth: true
            visible: !!rootItem.model && !!rootItem.model.root

            Label { text: qsTr("Text:") }
            TextField {
                Layout.fillWidth: true
                objectName: "expressionDslField"
                readOnly: true
                selectByMouse: true
                text: rootItem.model ? rootItem.model.dslText : ""
            }
        }
    }
}
