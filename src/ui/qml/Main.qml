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

    Settings {
        id: columnVisibilitySettings
        category: "graphColumns"
        property bool score: true
        property bool accuracy: true
        property bool shots: true
        property bool kills: true
        property bool dmg: true
        property bool scoreTotal: true
        property bool expectedFinalScore: true
        property bool expectedFinalScoreRecent: true
    }

    required property var graphVm
    required property var playtimeVm
    required property var sessionVm
    required property var settingsVm
    required property var scenarioBrowserVm

    FolderDialog {
        id: folderDialog
        currentFolder: root.settingsVm.kovaaksDir
        onAccepted: root.settingsVm.setKovaaksDir(folderDialog.selectedFolder)
    }

    SettingsDialog {
        id: settingsDialog
        settingsVm: root.settingsVm
        graphVm: root.graphVm
        columnVisibility: columnVisibilitySettings
    }

    menuBar: AppMenuBar {
        onSetSourceDirRequested: folderDialog.open()
        onSettingsRequested: settingsDialog.open()
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 5
        Label {
            Layout.row: 0; Layout.column: 0
            Layout.columnSpan: 3
            text: "Dashboard"
            font.pixelSize: 24
            font.bold: true
        }

        ColumnLayout {
            Layout.row: 1; Layout.column: 2
            Layout.fillWidth: true
            Layout.fillHeight: true

            DashboardGraphCanvas {
                Layout.fillWidth: true
                Layout.fillHeight: true
                graphVm: root.graphVm
                columnVisibility: columnVisibilitySettings
            }
            PlaytimeGraphPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                playtimeVm: root.playtimeVm
            }
        }
        ControlPanel {
            Layout.row: 1; Layout.column: 1
            sessionVm: root.sessionVm
            graphVm: root.graphVm
            columnVisibility: columnVisibilitySettings
        }
        SelectionPanel {
            Layout.row: 1; Layout.column: 0
            Layout.fillHeight: true
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
