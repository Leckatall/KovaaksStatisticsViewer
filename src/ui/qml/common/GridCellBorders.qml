import QtQuick
import KovaaksStatsViewer

Item {
    property bool showBottom: true

    anchors.fill: parent

    Rectangle {
        visible: parent.showBottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: palette.mid
    }
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: palette.mid
    }
}
