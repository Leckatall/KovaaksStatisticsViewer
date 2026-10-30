import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    objectName: "noGraphsShown"

    property string message
    property var visualSettings: null

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: qsTr("No graphs shown")
            font.pixelSize: 18
            font.bold: true
            Accessible.role: Accessible.Heading
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Qt.alpha(root.palette.text, 0.78)
            text: root.message
        }
        Button {
            id: chooseButton
            objectName: "chooseGraphsButton"
            text: qsTr("Choose graphs…")
            onClicked: chooseMenu.popup(chooseButton, 0, chooseButton.height)
        }
    }

    GraphVisibilityMenu {
        id: chooseMenu
        objectName: "chooseGraphsMenu"
        visualSettings: root.visualSettings
    }
}
