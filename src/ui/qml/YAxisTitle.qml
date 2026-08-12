import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property rect plotArea
    property string text: ""
    property string labelObjectName: "yAxisTitleLabel"

    Layout.preferredWidth: 16
    Layout.fillHeight: true
    clip: true

    Label {
        objectName: root.labelObjectName
        text: root.text
        font.pixelSize: 10
        color: Qt.alpha(root.palette.text, 0.6)
        rotation: -90
        width: root.plotArea.height
        height: root.width
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        x: root.width / 2 - root.plotArea.height / 2
        y: root.plotArea.y + root.plotArea.height / 2 - root.width / 2
    }
}
