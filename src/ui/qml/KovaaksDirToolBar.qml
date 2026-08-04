import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    required property url kovaaksDir
    // TODO: Also display the current profile name
    // required property string profileName

    RowLayout {
        anchors.fill: parent
        Label {
            objectName: "kovaaksDirLabel"
            text: "KovaaksDir: " + root.kovaaksDir + "\n"
            font.pixelSize: 18
            font.bold: true
            color: "white"
            Layout.leftMargin: 12
        }
    }
}
