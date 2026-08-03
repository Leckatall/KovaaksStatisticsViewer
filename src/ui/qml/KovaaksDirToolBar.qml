import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    required property url kovaaksDir

    RowLayout {
        anchors.fill: parent
        Label {
            text: "KovaaksDir: " + root.kovaaksDir
            font.pixelSize: 18
            font.bold: true
            color: "white"
            Layout.leftMargin: 12
        }
    }
}
