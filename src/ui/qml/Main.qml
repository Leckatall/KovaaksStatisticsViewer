import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
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

    Rectangle {
        anchors.fill: parent
        color: "#121212"
    }

    FolderDialog {
        id: folderDialog
        currentFolder: root.settingsVm.kovaaksDir
        onAccepted: root.settingsVm.setKovaaksDir(folderDialog.selectedFolder)
    }

    Loader {
        id: settingsDialogLoader
        active: false
        sourceComponent: SettingsDialog {
            settingsVm: root.settingsVm
            graphVm: root.graphVm
            columnVisibility: columnVisibilitySettings
        }
        onLoaded: item.open()
    }

    menuBar: AppMenuBar {
        onSetSourceDirRequested: folderDialog.open()
        onSettingsRequested: {
            if (settingsDialogLoader.active) settingsDialogLoader.item.open()
            else settingsDialogLoader.active = true
        }
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
            color: "white"
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
            onSearchEdited: text => root.scenarioBrowserVm.setSearchText(text)
            onScenarioActivated: (hash, name) => root.scenarioBrowserVm.activateScenario(hash, name)
        }
    }
}
