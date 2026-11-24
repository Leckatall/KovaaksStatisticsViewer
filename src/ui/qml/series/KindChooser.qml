pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

RowLayout {
    id: chooser

    required property var treeModel
    required property var kindLabel
    required property EditableExpressionNode parentNode
    property string slot: "root"

    Label { text: qsTr("Add:") }
    ComboBox {
        id: kindCombo

        Layout.fillWidth: true
        displayText: chooser.kindLabel(currentText)
        model: chooser.treeModel ? chooser.treeModel.nodeKinds : []
        objectName: "kindChooser_" + chooser.slot

        delegate: ItemDelegate {
            required property string modelData

            text: chooser.kindLabel(modelData)
            width: kindCombo.width
        }
    }
    Button {
        objectName: "addNodeButton_" + chooser.slot
        text: qsTr("Add")

        onClicked: if (chooser.treeModel) chooser.treeModel.replaceChild(chooser.parentNode, chooser.slot, kindCombo.currentText)
    }
}
