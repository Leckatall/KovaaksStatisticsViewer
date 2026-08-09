import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
// import QtCore
import KovaaksStatsViewer

// Dev component gallery. gallery_main.cpp exposes the real VMs through the
// engine's root context so they remain available when qmlpreview recreates
// this root object during live reload.
ApplicationWindow {
    id: window
    width: 1280
    height: 900
    visible: true
    title: "KSV Component Gallery"

    readonly property var graphVm: galleryGraphVm
    readonly property var playtimeVm: galleryPlaytimeVm
    readonly property var sessionVm: gallerySessionVm
    readonly property var settingsVm: gallerySettingsVm

    Material.theme: Material.Dark
    Material.accent: Material.Cyan
    Material.primary: Material.BlueGrey

    // Mirrors Main.qml's columnVisibilitySettings; a plain object is enough here.
    property var columnVisibility: ({
        score: true, accuracy: true, shots: true, kills: true, dmg: true,
        scoreTotal: true, expectedFinalScore: true, expectedFinalScoreRecent: true
    })

    background: Rectangle { color: "#121212" }

    // A labelled, sized cell. Components like ControlPanel have no implicit
    // size of their own, so every showcase gets explicit dimensions.
    component Showcase: ColumnLayout {
        default property alias content: holder.data
        property string label
        property real cellWidth: 560
        property real cellHeight: 320
        spacing: 6

        Label {
            text: parent.label
            color: "#7FDBFF"
            font.bold: true
            font.pixelSize: 15
        }
        Rectangle {
            Layout.preferredWidth: parent.cellWidth
            Layout.preferredHeight: parent.cellHeight
            color: "#181818"
            border.color: "#2A2A2A"
            radius: 8
            Item {
                id: holder
                anchors.fill: parent
                anchors.margins: 8
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: availableWidth

        Flow {
            width: window.width - 48
            spacing: 24

            Showcase {
                label: "DashboardGraphCanvas"
                cellWidth: 600; cellHeight: 360
                DashboardGraphCanvas {
                    anchors.fill: parent
                    graphVm: window.graphVm
                    columnVisibility: window.columnVisibility
                }
            }

            Showcase {
                label: "PlaytimeGraphPanel"
                cellWidth: 600; cellHeight: 360
                PlaytimeGraphPanel {
                    anchors.fill: parent
                    playtimeVm: window.playtimeVm
                }
            }

            Showcase {
                label: "ControlPanel"
                cellWidth: 440; cellHeight: 480
                ControlPanel {
                    anchors.fill: parent
                    sessionVm: window.sessionVm
                    graphVm: window.graphVm
                    columnVisibility: window.columnVisibility
                }
            }

            Showcase {
                label: "AppMenuBar"
                cellWidth: 440; cellHeight: 100
                AppMenuBar {
                    anchors.left: parent.left
                    anchors.right: parent.right
                }
            }

            Showcase {
                label: "DirectoryPickerRow"
                cellWidth: 440; cellHeight: 120
                DirectoryPickerRow {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    label: "Kovaaks Directory"
                    dir: window.settingsVm.kovaaksDir
                    objectNamePrefix: "galleryDir"
                }
            }

            Showcase {
                label: "SettingsDialog"
                cellWidth: 440; cellHeight: 120
                Button {
                    anchors.centerIn: parent
                    text: "Open SettingsDialog"
                    onClicked: settingsDialog.open()
                }
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
        settingsVm: window.settingsVm
        graphVm: window.graphVm
        columnVisibility: window.columnVisibility
    }
}
