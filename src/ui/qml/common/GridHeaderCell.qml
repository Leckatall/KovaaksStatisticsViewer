import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Rectangle {
    id: headerCell

    property string text
    property bool focused: false
    property int alignment: Text.AlignLeft
    property real labelPadding: 6

    color: palette.button
    border.color: focused ? palette.highlight : palette.mid
    border.width: focused ? 2 : 1

    Label {
        anchors.fill: parent
        anchors.leftMargin: headerCell.labelPadding
        anchors.rightMargin: headerCell.labelPadding
        visible: headerCell.text !== ""
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: headerCell.alignment
        elide: Text.ElideRight
        font.bold: true
        text: headerCell.text
    }
}
