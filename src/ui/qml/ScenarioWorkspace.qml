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
    // Whether a Kovaaks folder has been configured (SettingsViewModel.kovaaksDirSet).
    property bool kovaaksFolderSet: true

    // First run: no folder configured and nothing imported (e.g. from an existing
    // profile or the default Steam path). The workspace then shows a welcome with
    // choosing the folder as its one action instead of a screen of empty graphs.
    readonly property bool welcomeVisible: !kovaaksFolderSet && !scenarioBrowserVm.hasScenarios

    signal configureGraphLinesRequested()
    signal chooseKovaaksFolderRequested()

    FirstRunWelcome {
        objectName: "firstRunWelcome"
        anchors.fill: parent
        visible: root.welcomeVisible
        onChooseFolderRequested: root.chooseKovaaksFolderRequested()
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 5
        visible: !root.welcomeVisible

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
            activeScenarioHash: root.scenarioBrowserVm.activeScenarioHash
            activeScenarioName: root.scenarioBrowserVm.activeScenarioName
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
