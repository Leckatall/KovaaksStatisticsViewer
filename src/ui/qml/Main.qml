import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtGraphs

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
                anchors.margins: 12
                axisX: ValueAxis {
                    min: root.graphVm.xMin; max: root.graphVm.xMax
                }
                axisY: ValueAxis {
                    min: root.graphVm.yMin; max: root.graphVm.yMax; subTickCount: 4
                }
                LineSeries {
                    id: score_series
                    name: "Performance"
                    width: 3
                    pointDelegate: Rectangle {
                        id: delegate
                        width: 4; height: 4; radius: 4; color: "#4DD0E1"
                        property real pointValueY
                        HoverHandler {
                            id: hoverHandler
                            target: Text {
                                parent: delegate
                                visible: hoverHandler.hovered
                                text: `You hovering me! ${delegate.pointValueY.toFixed(2)}`
                            }
                        }

                    }


                    color: "#009600"
                    // joinStyle: Qt.RoundJoin

                    XYModelMapper {
                        series: score_series
                        model: root.graphVm
                        xSection: 0
                        ySection: 1
                    }

                }

                LineFromModel {
                    line_model: root.graphVm
                    xIndex: 0
                    yIndex: 2
                    color: "cyan"
                    width: 3
                    pointDelegate: Rectangle {
                        id: delegate2
                        width: 4; height: 4; radius: 4; color: "#4DD0E1"
                        property real pointValueY

                        HoverHandler {
                            id: hoverHandler2
                            target: Text {
                                parent: delegate2
                                visible: hoverHandler2.hovered
                                text: `You hovering me! ${delegate2.pointValueY.toFixed(2)}`
                            }
                        }

                    }
                }

            }

            Button {
                text: "Import Score of example performance"
                onClicked: root.graphVm.fetchData("Not yet implemented")
            }
        }
    }

}
