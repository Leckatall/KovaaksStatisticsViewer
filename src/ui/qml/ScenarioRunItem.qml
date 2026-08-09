import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ItemDelegate {
    id: root
    width: ListView.view ? ListView.view.width : implicitWidth
    padding: 10
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    objectName: "runItem_" + index

    required property int index
    required property string runLabel
    required property real score
    required property real accuracy
    required property string hash
    required property double startTimeMs
    signal runSelected(string hash, double startTimeMs)

    background: Rectangle {
        radius: 8
        color: root.down ? "#303030" : root.hovered ? "#282828" : "#202020"
        border.color: "#3A3A3A"
    }

    contentItem: ColumnLayout {
        spacing: 2

        Label {
            objectName: "runLabel_" + root.index
            Layout.fillWidth: true
            text: root.runLabel
            font.bold: true
            elide: Text.ElideRight
            color: Material.foreground
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                objectName: "runScore_" + root.index
                text: "Score " + root.score.toFixed(0)
                color: Material.foreground
            }
            Label {
                objectName: "runAccuracy_" + root.index
                text: "Acc " + (root.accuracy * 100).toFixed(1) + "%"
                color: Material.foreground
            }
        }
    }

    onClicked: root.runSelected(hash, startTimeMs)
}
