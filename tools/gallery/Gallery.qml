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
                label: "RunListView"
                cellWidth: 600; cellHeight: 360
                RunListView {
                    anchors.fill: parent
                    title: "1wall6targets TE"
                    runModel: ListModel {
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-09"; scenarioName: "1wall6targets TE"; startTimeMs: 1723200000000; score: 8421; accuracy: 0.91; durationSeconds: 60; shots: 132; hits: 120 }
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-08"; scenarioName: "1wall6targets TE"; startTimeMs: 1723113600000; score: 8150; accuracy: 0.885; durationSeconds: 59.8; shots: 128; hits: 113 }
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-07"; scenarioName: "1wall6targets TE"; startTimeMs: 1723027200000; score: 8630; accuracy: 0.932; durationSeconds: 60; shots: 137; hits: 128 }
                    }
                }
            }

            Showcase {
                label: "ScenarioSearchPanel"
                cellWidth: 440; cellHeight: 360
                ScenarioSearchPanel {
                    anchors.fill: parent
                    scenarioModel: ListModel {
                        ListElement { name: "1wall6targets TE"; hash: "te"; runCount: 42; lastPlayedMs: 1723200000000 }
                        ListElement { name: "Pasu Small Reload"; hash: "psr"; runCount: 19; lastPlayedMs: 1723113600000 }
                        ListElement { name: "Smoothbot Invincible"; hash: "smooth"; runCount: 8; lastPlayedMs: 1723027200000 }
                    }
                }
            }

            Showcase {
                label: "SelectionPanel"
                cellWidth: 600; cellHeight: 700
                SelectionPanel {
                    anchors.fill: parent
                    activeScenarioName: "1wall6targets TE"
                    scenarioModel: ListModel {
                        ListElement { name: "1wall6targets TE"; hash: "te"; runCount: 42; lastPlayedMs: 1723200000000 }
                        ListElement { name: "Pasu Small Reload"; hash: "psr"; runCount: 19; lastPlayedMs: 1723113600000 }
                    }
                    runModel: ListModel {
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-09"; scenarioName: "1wall6targets TE"; startTimeMs: 1723200000000; score: 8421; accuracy: 0.91; durationSeconds: 60; shots: 132; hits: 120 }
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-08"; scenarioName: "1wall6targets TE"; startTimeMs: 1723113600000; score: 8150; accuracy: 0.885; durationSeconds: 59.8; shots: 128; hits: 113 }
                    }
                    recentRunModel: ListModel {
                        ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-09"; scenarioName: "1wall6targets TE"; startTimeMs: 1723200000000; score: 8421; accuracy: 0.91; durationSeconds: 60; shots: 132; hits: 120 }
                        ListElement { hash: "psr"; runLabel: "Pasu Small Reload 2024-08-08"; scenarioName: "Pasu Small Reload"; startTimeMs: 1723113600000; score: 7120; accuracy: 0.84; durationSeconds: 45; shots: 98; hits: 82 }
                    }
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
