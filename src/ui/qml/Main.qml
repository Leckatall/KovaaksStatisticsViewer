import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

ApplicationWindow {
    id: root
    width: 1200
    height: 800

    visible: true
    title: "Kovaaks Stats Viewer"
    // The app's accent, propagated to every control and popup below. The dark
    // scheme itself is pinned in main.cpp, not here.
    palette.accent: "#00BCD4"
    palette.highlight: "#00838F"

    Settings {
        category: "window"
        property alias width: root.width
        property alias height: root.height
    }
    QtObject {
        id: settings
        property list<int> test: [1, 2, 3]

    }
    Settings {
        id: seriesVisibilitySettings
        category: "graphSeriesVisibility"
        property var hiddenSeriesIds: []
    }

    Settings {
        id: graphAxisSettings
        category: "graphAxis"
        property string seriesId: ""
    }

    Settings {
        id: legacyGraphAxisSettings
        category: "graph"
        property string yAxisColumnKey: "score"
    }

    Settings {
        id: historyColumnVisibilitySettings
        category: "historyGraphColumns"
        property bool score: true
        property bool accuracy: false
        property bool shots: false
        property bool hits: false
        property bool misses: false
    }

    Settings {
        id: historyAxisSettings
        category: "historyGraph"
        property string yAxisColumnKey: "score"
    }

    Settings {
        id: viewSettings
        category: "view"
        property bool scenarioGraphVisible: true
        property bool playtimeGraphVisible: true
        property bool scenarioHistoryGraphVisible: true
        property bool controlPanelVisible: true
        property bool selectionPanelVisible: true
        property bool recentRunsSectionVisible: true
        property bool scenarioBrowserSectionVisible: true
    }

    required property var graphVm
    required property var playtimeVm
    required property var historyVm
    required property var sessionVm
    required property var settingsVm
    required property var scenarioBrowserVm

    FolderDialog {
        id: folderDialog
        objectName: "kovaaksFolderDialog"
        currentFolder: root.settingsVm.kovaaksDir
        onAccepted: root.settingsVm.setKovaaksDir(folderDialog.selectedFolder)
    }

    FileDialog {
        id: perfFileDialog
        title: "Load Performance"
        nameFilters: ["Performance Files (*.perf)"]
        onAccepted: root.graphVm.fetchData(perfFileDialog.selectedFiles[0])
    }

    SettingsDialog {
        id: settingsDialog
        settingsVm: root.settingsVm
        sessionVm: root.sessionVm
        graphVm: root.graphVm
        seriesVisibility: seriesVisibilitySettings
        graphAxisSettings: graphAxisSettings
        legacyGraphAxisSettings: legacyGraphAxisSettings
    }

    AboutDialog {
        id: aboutDialog
    }

    menuBar: AppMenuBar {
        graphVm: root.graphVm
        historyVm: root.historyVm
        seriesVisibility: seriesVisibilitySettings
        historyColumnVisibility: historyColumnVisibilitySettings
        viewSettings: viewSettings
        onSetSourceDirRequested: folderDialog.open()
        onSettingsRequested: settingsDialog.open()
        onConfigureGraphLinesRequested: settingsDialog.openGraphLines()
        onLoadPerformanceFileRequested: perfFileDialog.open()
        onQuitRequested: Qt.quit()
        onAboutRequested: aboutDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent

        FirstRunBanner {
            objectName: "firstRunBanner"
            Layout.fillWidth: true
            visible: !root.settingsVm.kovaaksDirSet
            onChooseFolderRequested: folderDialog.open()
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 5

            ColumnLayout {
                Layout.row: 1; Layout.column: 2
                Layout.fillWidth: true
                Layout.fillHeight: true

                DashboardGraphCanvas {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: viewSettings.scenarioGraphVisible
                    graphVm: root.graphVm
                    seriesVisibility: seriesVisibilitySettings
                    graphAxisSettings: graphAxisSettings
                }
                PlaytimeGraphPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: viewSettings.playtimeGraphVisible
                    playtimeVm: root.playtimeVm
                }
                ScenarioHistoryPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: viewSettings.scenarioHistoryGraphVisible
                    historyVm: root.historyVm
                    columnVisibility: historyColumnVisibilitySettings
                    historyAxisSettings: historyAxisSettings
                }
            }
            ControlPanel {
                Layout.row: 1; Layout.column: 1
                visible: viewSettings.controlPanelVisible
                graphVm: root.graphVm
                seriesVisibility: seriesVisibilitySettings
                onConfigureLinesRequested: settingsDialog.openGraphLines()
            }
            SelectionPanel {
                Layout.row: 1; Layout.column: 0
                Layout.fillHeight: true
                visible: viewSettings.selectionPanelVisible
                recentSectionVisible: viewSettings.recentRunsSectionVisible
                scenarioBrowserSectionVisible: viewSettings.scenarioBrowserSectionVisible
                widestScenarioName: root.scenarioBrowserVm.longestScenarioName
                maximumPanelWidth: root.width / 3
                currentRunHash: root.scenarioBrowserVm.currentRunHash
                currentRunStartTimeMs: root.scenarioBrowserVm.currentRunStartTimeMs
                scenarioModel: root.scenarioBrowserVm.scenarioModel
                runModel: root.scenarioBrowserVm.runModel
                recentRunModel: root.scenarioBrowserVm.recentRunsModel
                onSearchEdited: text => root.scenarioBrowserVm.setSearchText(text)
                onScenarioActivated: (hash, name) => root.scenarioBrowserVm.activateScenario(hash, name)
                onRunSelected: (hash, startTimeMs) => root.scenarioBrowserVm.selectRun(hash, startTimeMs)
                onSortRequested: (field, ascending) => root.scenarioBrowserVm.setRunSort(field, ascending)
                onScenarioSortRequested: (field, ascending) => root.scenarioBrowserVm.setScenarioSort(field, ascending)
            }
        }
    }
}
