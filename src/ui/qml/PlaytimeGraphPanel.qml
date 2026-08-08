import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var playtimeVm

    Layout.fillHeight: true
    Layout.fillWidth: true

    background: Rectangle {
        border.color: "#2A2A2A"
        color: "#1E1E1E"
        radius: 12
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            color: "white"
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "playtimeTitleLabel"
            text: "Daily Playtime (3-day rolling average)"
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            GraphCanvas {
                id: canvas

                anchors.fill: parent
                graphVm: root.playtimeVm
                // Single series ("Playtime"), always visible.
                visibleColumns: [true]
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

                color: "#80FFFFFF"
                height: plotArea.height
                visible: hoverArea.hoverInfo.valid === true
                width: 1
                x: hoverArea.hoverInfo.valid ? hoverArea.hoverInfo.pixelX : 0
                y: plotArea.y
                z: 100
            }
            Rectangle {
                id: tooltipBox

                // hoverInfo.time is a day count since the Unix epoch; render it
                // as a UTC calendar date to match the graph's X axis.
                readonly property string dateLabel: hoverArea.hoverInfo.valid ? new Date(hoverArea.hoverInfo.time * 86400000).toLocaleDateString(Qt.locale(), Locale.ShortFormat) : ""
                property real idealX: hoverArea.mouseX + 14
                property real idealY: hoverArea.mouseY - height / 2

                color: "#DD222222"
                border.color: "#555555"
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
                        color: "#CCCCCC"
                        font.pixelSize: 11
                        font.bold: true
                        text: tooltipBox.dateLabel
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
                                color: "white"
                                font.pixelSize: 11
                                text: modelData.value.toFixed(1) + " min"
                            }
                        }
                    }
                }
            }
        }
    }
}
