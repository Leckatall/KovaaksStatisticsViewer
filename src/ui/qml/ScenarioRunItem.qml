import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: root

    required property real accuracy
    readonly property color currentRunColor: Qt.tint(palette.base, Qt.rgba(palette.accent.r, palette.accent.g, palette.accent.b, 0.05))
    property string currentRunHash
    property double currentRunStartTimeMs: 0
    required property string hash
    required property int index
    readonly property bool isCurrentRun: hash === currentRunHash && startTimeMs === currentRunStartTimeMs
    required property string runLabel
    required property real score
    required property double startTimeMs

    signal runSelected(string hash, double startTimeMs)

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    objectName: "runItem_" + index
    padding: 10
    width: ListView.view ? ListView.view.width : implicitWidth

    background: Rectangle {
        readonly property color baseColor: root.isCurrentRun ? root.currentRunColor : root.palette.base

        color: Qt.lighter(baseColor, root.down ? 1.4 : root.hovered ? 1.2 : 1.0)

        border.color: root.isCurrentRun ? root.palette.accent : root.palette.mid
        radius: 8
    }
    contentItem: ColumnLayout {
        spacing: 2

        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.bold: true
            objectName: "runLabel_" + root.index
            text: root.runLabel
        }
        RowLayout {
            Layout.fillWidth: true

            Label {
                objectName: "runScore_" + root.index
                text: "Score " + root.score.toFixed(0)
            }
            Label {
                objectName: "runAccuracy_" + root.index
                text: "Acc " + (root.accuracy * 100).toFixed(1) + "%"
            }
        }
    }

    onClicked: root.runSelected(hash, startTimeMs)
}
