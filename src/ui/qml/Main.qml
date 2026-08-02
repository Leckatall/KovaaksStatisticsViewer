import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtGraphs
import QtCore

ApplicationWindow {
    id: root
    width: 1200
    height: 800

    visible: true
    title: "Kovaaks Stats Viewer"
    Material.theme: Material.Dark
    Material.accent: Material.Cyan
    Material.primary: Material.BlueGrey

    Settings {
        category: "window"
        property alias width: root.width
        property alias height: root.height
    }
    Settings {
        id: fileSettings
        category: "file"
        property url kovaaks
    }

    required property var graphVm
    required property var sessionVm

    Rectangle {
        anchors.fill: parent
        color: "#121212"
    }

    FolderDialog {
        id: folderDialog
        currentFolder: fileSettings.kovaaks
        onAccepted: fileSettings.kovaaks = folderDialog.selectedFolder
    }


    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&New...")
            }
            Action {
                id: setSoruceDirAction
                text: qsTr("Set Source Directory")
                onTriggered: folderDialog.open()
            }

            Action {
                text: qsTr("&Save")
            }
            Action {
                text: qsTr("Save &As...")
            }
            Action {
                text: qsTr("Settings")
            }
            MenuSeparator {
            }
            Action {
                text: qsTr("&Quit")
            }
        }
        Menu {
            title: qsTr("&Help")
            Action {
                text: qsTr("&About")
            }
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: "KovaaksDir: " + fileSettings.kovaaks
                font.pixelSize: 18
                font.bold: true
                color: "white"
                Layout.leftMargin: 12
            }
        }
    }
    GridLayout {
        anchors.fill: parent
        anchors.margins: 5
        Label {
            Layout.row: 0; Layout.column: 0
            Layout.columnSpan: 2
            text: "Dashboard"
            font.pixelSize: 24
            font.bold: true
            color: "white"
        }

        Frame {
            Layout.row: 1; Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: 12
                color: "#1E1E1E"
                border.color: "#2A2A2A"
            }

            GraphsView {
                anchors.fill: parent
                anchors.margins: 1
                axisX: ValueAxis {
                    min: root.graphVm.xMin; max: root.graphVm.xMax
                }
                axisY: ValueAxis {
                    min: root.graphVm.yMin; max: root.graphVm.yMax; subTickCount: 4
                }
                LineFromModel {
                    line_model: root.graphVm
                    xIndex: 0
                    yIndex: 1
                    color: "#009600"
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
        }
        Frame {
            Layout.row: 1; Layout.column: 0
            ColumnLayout {
                ComboBox {
                    id: scenarioComboBox
                    model: root.sessionVm.scenario_list
                    // editable: true
                    Layout.fillWidth: true
                }
                Button {
                    text: "Generate Profile from current kovaaks dir"
                    onClicked: root.sessionVm.generateProfile()
                }
                Button {
                    text: "update comboBox"
                    onClicked: {
                        scenarioComboBox.model = root.sessionVm.scenario_list
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
