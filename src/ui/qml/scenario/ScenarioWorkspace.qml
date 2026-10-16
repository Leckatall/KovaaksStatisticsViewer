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
    property var settingsVm: null
    // Whether a Kovaaks folder has been configured (SettingsViewModel.kovaaksDirSet).
    property bool kovaaksFolderSet: true

    // First run: no folder configured and nothing imported (e.g. from an existing
    // profile or the default Steam path). The workspace then shows a welcome with
    // choosing the folder as its one action instead of a screen of empty graphs.
    readonly property bool welcomeVisible: !kovaaksFolderSet && !scenarioBrowserVm.hasScenarios

    signal configureGraphLinesRequested()
    signal moreGraphSettingsRequested(string graphKey)
    signal chooseKovaaksFolderRequested()

    // Ctrl+F: show the scenario browser if it is hidden, then focus its search field. Nothing to
    // search on the first-run welcome.
    function focusScenarioSearch() {
        if (root.welcomeVisible) return
        root.visualSettings.selectionPanelVisible = true
        root.visualSettings.scenarioBrowserSectionVisible = true
        Qt.callLater(selectionPanel.focusScenarioSearch)
    }

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

        // The graph panels split the column equally whatever their content: fill items share the
        // space in the ratio of their preferred heights, so each states the same one. A hidden
        // panel drops out and the others share its space. The two panels about the selected scenario
        // come first, then the all-scenario playtime panel.
        ColumnLayout {
            Layout.row: 1; Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            objectName: "graphPanelColumn"

            DashboardGraphCanvas {
                objectName: "scenarioGraphPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                visible: root.visualSettings.scenarioGraphVisible
                graphVm: root.graphVm
                visualSettings: root.visualSettings
                settingsVm: root.settingsVm
                onConfigureLinesRequested: root.configureGraphLinesRequested()
                onMoreGraphSettingsRequested: key => root.moreGraphSettingsRequested(key)
            }
            ScenarioHistoryPanel {
                objectName: "scenarioHistoryPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                visible: root.visualSettings.scenarioHistoryGraphVisible
                historyVm: root.historyVm
                columnVisibility: root.visualSettings.historyColumnVisibility
                historyAxisSettings: root.visualSettings
                settingsVm: root.settingsVm
                onMoreGraphSettingsRequested: key => root.moreGraphSettingsRequested(key)
            }
            PlaytimeGraphPanel {
                objectName: "playtimeGraphPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                visible: root.visualSettings.playtimeGraphVisible
                playtimeVm: root.playtimeVm
                settingsVm: root.settingsVm
                onMoreGraphSettingsRequested: key => root.moreGraphSettingsRequested(key)
            }
        }
        SelectionPanel {
            id: selectionPanel
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
