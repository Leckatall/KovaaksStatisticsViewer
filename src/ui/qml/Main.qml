import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

ApplicationWindow {
    id: root
    width: visualSettings.windowWidth
    height: visualSettings.windowHeight
    onWidthChanged: visualSettings.windowWidth = width
    onHeightChanged: visualSettings.windowHeight = height

    visible: true
    title: "Kovaaks Stats Viewer"
    // The app's accent, propagated to every control and popup below. The dark
    // scheme itself is pinned in main.cpp, not here.
    palette.accent: "#00BCD4"
    palette.highlight: "#00838F"

    VisualSettingsManager {
        id: visualSettings
        objectName: "visualSettings"
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
        visualSettings: visualSettings
    }

    AboutDialog {
        id: aboutDialog
    }

    menuBar: AppMenuBar {
        graphVm: root.graphVm
        historyVm: root.historyVm
        visualSettings: visualSettings
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
                    visible: visualSettings.scenarioGraphVisible
                    graphVm: root.graphVm
                    visualSettings: visualSettings
                }
                PlaytimeGraphPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: visualSettings.playtimeGraphVisible
                    playtimeVm: root.playtimeVm
                }
                ScenarioHistoryPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: visualSettings.scenarioHistoryGraphVisible
                    historyVm: root.historyVm
                    columnVisibility: visualSettings.historyColumnVisibility
                    historyAxisSettings: visualSettings
                }
            }
            ControlPanel {
                Layout.row: 1; Layout.column: 1
                visible: visualSettings.controlPanelVisible
                graphVm: root.graphVm
                visualSettings: visualSettings
                onConfigureLinesRequested: settingsDialog.openGraphLines()
            }
            SelectionPanel {
                Layout.row: 1; Layout.column: 0
                Layout.fillHeight: true
                visible: visualSettings.selectionPanelVisible
                recentSectionVisible: visualSettings.recentRunsSectionVisible
                scenarioBrowserSectionVisible: visualSettings.scenarioBrowserSectionVisible
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

    Connections {
        target: root.graphVm
        function onSeriesConfigurationChanged() {
            visualSettings.syncVisibleSeriesIds(root.graphVm.enabledSeriesIds)
        }
    }

    Component.onCompleted: visualSettings.syncVisibleSeriesIds(root.graphVm.enabledSeriesIds)
}
