import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Item {
    id: root

    required property var graphVm
    required property var playtimeVm
    required property var historyVm
    required property var scenarioBrowserVm
    required property var visualSettings

    signal configureGraphLinesRequested()

    GridLayout {
        anchors.fill: parent
        anchors.margins: 5

        ColumnLayout {
            Layout.row: 1; Layout.column: 2
            Layout.fillWidth: true
            Layout.fillHeight: true

            DashboardGraphCanvas {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.visualSettings.scenarioGraphVisible
                graphVm: root.graphVm
                visualSettings: root.visualSettings
            }
            PlaytimeGraphPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.visualSettings.playtimeGraphVisible
                playtimeVm: root.playtimeVm
            }
            ScenarioHistoryPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.visualSettings.scenarioHistoryGraphVisible
                historyVm: root.historyVm
                columnVisibility: root.visualSettings.historyColumnVisibility
                historyAxisSettings: root.visualSettings
            }
        }
        ControlPanel {
            Layout.row: 1; Layout.column: 1
            visible: root.visualSettings.controlPanelVisible
            graphVm: root.graphVm
            visualSettings: root.visualSettings
            onConfigureLinesRequested: root.configureGraphLinesRequested()
        }
        SelectionPanel {
            Layout.row: 1; Layout.column: 0
            Layout.fillHeight: true
            visible: root.visualSettings.selectionPanelVisible
            recentSectionVisible: root.visualSettings.recentRunsSectionVisible
            scenarioBrowserSectionVisible: root.visualSettings.scenarioBrowserSectionVisible
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
