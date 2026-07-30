import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtGraphs
import KovaaksStatsViewer

ApplicationWindow {
    id: root
    width: 1200
    height: 800
    visible: true
    title: "Kovaaks Stats Viewer"
    Material.theme: Material.Dark
    Material.accent: Material.Cyan
    Material.primary: Material.BlueGrey

    required property var graphVm

    Rectangle {
        anchors.fill: parent
        color: "#121212"
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: "Kovaaks Stats Viewer"
                font.pixelSize: 18
                font.bold: true
                color: "white"
                Layout.leftMargin: 12
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "Dashboard"
            font.pixelSize: 24
            font.bold: true
            color: "white"
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: 12
                color: "#1E1E1E"
                border.color: "#2A2A2A"
            }
            GraphsView {
                anchors.fill: parent
                axisX: ValueAxis {
                    min: root.graphVm.xMin; max: root.graphVm.xMax
                }
                axisY: ValueAxis {
                    min: root.graphVm.yMin; max: root.graphVm.yMax; subTickCount: 4
                }
                LineSeries {
                    id: lineSeries
                    name: "Performance"
                    width: 2
                    pointDelegate: Rectangle {   // optional per-point marker
                        width: 4; height: 4; radius: 4; color: "#4DD0E1"
                    }
                    color: "#FFFFFF"
                }
                XYModelMapper {
                    series: lineSeries
                    model: graphVm
                    xSection: 0
                    ySection: 1
                }
            }
            Component.onCompleted: {
                graphVm.appendPoint(4, 4.2)
                graphVm.appendPoint(5, 6.1)
            }
        }
    }

}
