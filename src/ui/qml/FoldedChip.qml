import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: chip

    required property var childNode
    required property var treeModel
    required property var accentColorFn

    signal selected

    border.color: chip.childNode ? chip.accentColorFn(chip.childNode.kind, 0.45) : "transparent"
    color: chip.childNode ? chip.accentColorFn(chip.childNode.kind, 0.12) : "transparent"
    implicitHeight: content.implicitHeight + 12
    radius: 6

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: 6

        Label {
            Layout.fillWidth: true
            text: chip.childNode && chip.treeModel ? chip.treeModel.describe(chip.childNode) : ""
            wrapMode: Text.WordWrap
        }
        Label {
            opacity: 0.6
            text: "›"
        }
    }
    MouseArea {
        anchors.fill: parent

        onClicked: chip.selected()
    }
}
