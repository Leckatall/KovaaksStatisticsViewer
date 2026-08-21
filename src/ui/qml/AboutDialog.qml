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
            text: Qt.application.name
            font.bold: true
            font.pixelSize: 16
        }
        Label {
            text: qsTr("Version %1").arg(Qt.application.version)
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
