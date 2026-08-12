import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "aboutDialog"
    title: qsTr("About")
    modal: false
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: 320

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: "Kovaaks Stats Viewer"
            font.bold: true
            font.pixelSize: 16
        }
        Label {
            text: qsTr("Version information is not wired up yet.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
