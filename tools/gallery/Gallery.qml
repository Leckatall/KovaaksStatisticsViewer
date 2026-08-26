import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "expression_tree_mockup" as ExpressionTreeMockup
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
    readonly property var scenarioBrowserVm: galleryScenarioBrowserVm

    // Mirrors Main.qml's accent.
    palette.accent: "#00BCD4"
    palette.highlight: "#00838F"

    // Mirrors Main.qml's columnVisibilitySettings; a plain object is enough here.
    property var columnVisibility: ({
        score: true, accuracy: true, shots: true, kills: true, dmg: true,
        scoreTotal: true, expectedFinalScore: true, expectedFinalScoreRecent: true
    })

    // Mirrors Main.qml's graphAxisSettings; a plain object is enough here.
    property var graphAxisSettings: ({yAxisColumnKey: "score"})

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
            color: window.palette.accent
            font.bold: true
            font.pixelSize: 15
        }
        Rectangle {
            Layout.preferredWidth: parent.cellWidth
            Layout.preferredHeight: parent.cellHeight
            color: window.palette.base
            border.color: window.palette.mid
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

            // TODO(2026-08-26): Restore once gallery showcase work resumes — parked, not replaced;
            // no live equivalent covers these components in the gallery in the meantime
            // Showcase {
            //     label: "DashboardGraphCanvas"
            //     cellWidth: 600; cellHeight: 360
            //     DashboardGraphCanvas {
            //         anchors.fill: parent
            //         graphVm: window.graphVm
            //         columnVisibility: window.columnVisibility
            //         graphAxisSettings: window.graphAxisSettings
            //     }
            // }

            // Showcase {
            //     label: "PlaytimeGraphPanel"
            //     cellWidth: 600; cellHeight: 360
            //     PlaytimeGraphPanel {
            //         anchors.fill: parent
            //         playtimeVm: window.playtimeVm
            //     }
            // }

            // Showcase {
            //     label: "ControlPanel"
            //     cellWidth: 440; cellHeight: 480
            //     ControlPanel {
            //         anchors.fill: parent
            //         graphVm: window.graphVm
            //         columnVisibility: window.columnVisibility
            //     }
            // }

            // Showcase {
            //     label: "RunListView"
            //     cellWidth: 600; cellHeight: 360
            //     RunListView {
            //         anchors.fill: parent
            //         title: window.scenarioBrowserVm.activeScenarioHash === "" ? "" : "Active scenario runs"
            //         runModel: window.scenarioBrowserVm.runModel
            //         currentRunHash: window.scenarioBrowserVm.currentRunHash
            //         currentRunStartTimeMs: window.scenarioBrowserVm.currentRunStartTimeMs
            //         onRunSelected: (hash, startTimeMs) => window.scenarioBrowserVm.selectRun(hash, startTimeMs)
            //         onSortRequested: (field, ascending) => window.scenarioBrowserVm.setSort(field, ascending)
            //     }
            //
            //     // Preselects the first scenario so the showcase above renders real run data;
            //     // this Repeater is never shown, it only exists to read the first model row.
            //     Repeater {
            //         model: window.scenarioBrowserVm.scenarioModel
            //         delegate: Item {
            //             required property int index
            //             required property string name
            //             required property string hash
            //             Component.onCompleted: {
            //                 if (index === 0) window.scenarioBrowserVm.activateScenario(hash, name)
            //             }
            //         }
            //     }
            // }
            //
            // Showcase {
            //     label: "RunListView (Recent, all scenarios)"
            //     cellWidth: 600; cellHeight: 360
            //     RunListView {
            //         anchors.fill: parent
            //         title: "Recent"
            //         runModel: window.scenarioBrowserVm.recentRunsModel
            //         currentRunHash: window.scenarioBrowserVm.currentRunHash
            //         currentRunStartTimeMs: window.scenarioBrowserVm.currentRunStartTimeMs
            //         onRunSelected: (hash, startTimeMs) => window.scenarioBrowserVm.selectRun(hash, startTimeMs)
            //     }
            // }
            //
            // Showcase {
            //     label: "ScenarioSearchPanel"
            //     cellWidth: 440; cellHeight: 360
            //     ScenarioSearchPanel {
            //         anchors.fill: parent
            //         scenarioModel: window.scenarioBrowserVm.scenarioModel
            //         onSearchEdited: text => window.scenarioBrowserVm.setSearchText(text)
            //         onScenarioActivated: (hash, name) => window.scenarioBrowserVm.activateScenario(hash, name)
            //     }
            // }
            //
            // Showcase {
            //     label: "SelectionPanel"
            //     cellWidth: 600; cellHeight: 700
            //     SelectionPanel {
            //         anchors.fill: parent
            //         activeScenarioName: "1wall6targets TE"
            //         scenarioModel: ListModel {
            //             ListElement { name: "1wall6targets TE"; hash: "te"; runCount: 42; lastPlayedMs: 1723200000000 }
            //             ListElement { name: "Pasu Small Reload"; hash: "psr"; runCount: 19; lastPlayedMs: 1723113600000 }
            //         }
            //         runModel: ListModel {
            //             ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-09"; startTimeMs: 1723200000000; score: 8421; accuracy: 0.91; shots: 132; hits: 120 }
            //             ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-08"; startTimeMs: 1723113600000; score: 8150; accuracy: 0.885; shots: 128; hits: 113 }
            //         }
            //         recentRunModel: ListModel {
            //             ListElement { hash: "te"; runLabel: "1wall6targets TE 2024-08-09"; startTimeMs: 1723200000000; score: 8421; accuracy: 0.91; shots: 132; hits: 120 }
            //             ListElement { hash: "psr"; runLabel: "Pasu Small Reload 2024-08-08"; startTimeMs: 1723113600000; score: 7120; accuracy: 0.84; shots: 98; hits: 82 }
            //         }
            //     }
            // }
            //
            // Showcase {
            //     label: "AppMenuBar"
            //     cellWidth: 440; cellHeight: 100
            //     AppMenuBar {
            //         anchors.left: parent.left
            //         anchors.right: parent.right
            //     }
            // }
            //
            // Showcase {
            //     label: "DirectoryPickerRow"
            //     cellWidth: 440; cellHeight: 120
            //     DirectoryPickerRow {
            //         anchors.left: parent.left
            //         anchors.right: parent.right
            //         anchors.verticalCenter: parent.verticalCenter
            //         label: "Kovaaks Directory"
            //         dir: window.settingsVm.kovaaksDir
            //         objectNamePrefix: "galleryDir"
            //     }
            // }
            //
            // Showcase {
            //     label: "SettingsDialog"
            //     cellWidth: 440; cellHeight: 120
            //     Button {
            //         anchors.centerIn: parent
            //         text: "Open SettingsDialog"
            //         onClicked: settingsDialog.open()
            //     }
            // }

            Showcase {
                label: "Expression Tree — Cards (mockup)"
                cellWidth: 640; cellHeight: 520
                ExpressionTreeMockup.ExpressionTreeMockModel { id: cardsTreeModel }
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: cardsTreeModel.exampleNames
                            delegate: Button {
                                required property int index
                                required property string modelData
                                text: modelData
                                onClicked: cardsTreeModel.loadExample(index)
                            }
                        }
                    }
                    ExpressionTreeMockup.ExpressionTreeCardsMockup {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: cardsTreeModel
                    }
                }
            }

            Showcase {
                label: "Expression Tree — Breadcrumb (mockup)"
                cellWidth: 640; cellHeight: 520
                ExpressionTreeMockup.ExpressionTreeMockModel { id: breadcrumbTreeModel }
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: breadcrumbTreeModel.exampleNames
                            delegate: Button {
                                required property int index
                                required property string modelData
                                text: modelData
                                onClicked: breadcrumbTreeModel.loadExample(index)
                            }
                        }
                    }
                    ExpressionTreeMockup.ExpressionTreeBreadcrumbMockup {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: breadcrumbTreeModel
                    }
                }
            }
        }
    }

    // TODO(2026-08-26): Restore once gallery SettingsDialog wiring is updated to match current
    // SettingsDialog.qml properties — parked, not replaced; the gallery has no SettingsDialog
    // showcase in the meantime
    // SettingsDialog {
    //     id: settingsDialog
    //     settingsVm: window.settingsVm
    //     sessionVm: window.sessionVm
    //     graphVm: window.graphVm
    //     columnVisibility: window.columnVisibility
    //     graphAxisSettings: window.graphAxisSettings
    // }
}
