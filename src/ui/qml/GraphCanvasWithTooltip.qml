import QtQuick
import QtQuick.Layouts
import KovaaksStatsViewer

Item {
    id: root

    required property var graphVm
    property var visibleColumns: []
    property int yAxisColumn: -1
    property bool showSeriesNames: true
    readonly property rect plotArea: canvas.plotArea
    readonly property int labelledYAxisColumn: canvas.labelledYAxisColumn

    Layout.fillHeight: true
    Layout.fillWidth: true

    GraphCanvas {
        id: canvas

        anchors.fill: parent
        graphVm: root.graphVm
        visibleColumns: root.visibleColumns
        yAxisColumn: root.yAxisColumn
    }
    MouseArea {
        id: hoverArea

        property var hoverInfo: ({
                valid: false
            })

        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        hoverEnabled: true

        onExited: hoverInfo = {
            valid: false
        }
        onPositionChanged: mouse => hoverInfo = canvas.valuesAtX(mouse.x)
    }
    Rectangle {
        id: verticalLine

        readonly property rect plotArea: canvas.plotArea

        color: Qt.alpha(root.palette.text, 0.5)
        height: plotArea.height
        visible: hoverArea.hoverInfo.valid === true
        width: 1
        x: hoverArea.hoverInfo.valid ? hoverArea.hoverInfo.pixelX : 0
        y: plotArea.y
        z: 100
    }
    Rectangle {
        id: tooltipBox

        property real idealX: hoverArea.mouseX + 14
        property real idealY: hoverArea.mouseY - height / 2

        color: Qt.alpha(root.palette.toolTipBase, 0.87)
        border.color: root.palette.mid
        border.width: 1
        radius: 6
        visible: hoverArea.hoverInfo.valid === true
        width: tooltipContent.implicitWidth + 16
        height: tooltipContent.implicitHeight + 12
        x: (idealX + width > parent.width) ? hoverArea.mouseX - width - 14 : idealX
        y: Math.max(0, Math.min(idealY, parent.height - height))

        Column {
            id: tooltipContent

            anchors.centerIn: parent
            spacing: 3

            Text {
                color: Qt.alpha(root.palette.toolTipText, 0.8)
                font.pixelSize: 11
                font.bold: true
                text: hoverArea.hoverInfo.valid ? (root.showSeriesNames ? "Time: " + hoverArea.hoverInfo.x : hoverArea.hoverInfo.x) : ""
            }
            Repeater {
                model: hoverArea.hoverInfo.valid ? hoverArea.hoverInfo.series : []

                Row {
                    spacing: 5

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 2
                        color: modelData.color
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        color: root.palette.toolTipText
                        font.pixelSize: 11
                        text: root.showSeriesNames ? modelData.name + ": " + modelData.value : modelData.value
                    }
                }
            }
        }
    }
}
