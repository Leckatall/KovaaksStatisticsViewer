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
    required property var benchmarkManagerVm
    required property var benchmarkTrackingVm

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

    // Reachable as `mainWindow.benchmarkManagerDialog` because the manager is a
    // separate ApplicationWindow, not part of this window's item tree.
    property alias benchmarkManagerDialog: benchmarkManagerDialog

    BenchmarkManagerDialog {
        id: benchmarkManagerDialog
        objectName: "benchmarkManagerDialog"
        benchmarkManagerVm: root.benchmarkManagerVm
    }

    menuBar: AppMenuBar {
        graphVm: root.graphVm
        historyVm: root.historyVm
        visualSettings: visualSettings
        onSetSourceDirRequested: folderDialog.open()
        onSettingsRequested: settingsDialog.open()
        onConfigureGraphLinesRequested: settingsDialog.openGraphLines()
        onLoadPerformanceFileRequested: perfFileDialog.open()
        onManageBenchmarksRequested: benchmarkManagerDialog.open()
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

        TabBar {
            id: workspaceTabBar
            objectName: "workspaceTabBar"
            Layout.fillWidth: true
            focusPolicy: Qt.StrongFocus
            Keys.priority: Keys.BeforeItem
            Keys.onRightPressed: event => {
                workspaceTabBar.currentIndex =
                    Math.min(workspaceTabBar.currentIndex + 1, workspaceTabBar.count - 1)
                event.accepted = true
            }
            Keys.onLeftPressed: event => {
                workspaceTabBar.currentIndex = Math.max(workspaceTabBar.currentIndex - 1, 0)
                event.accepted = true
            }

            TabButton {
                objectName: "scenariosTab"
                text: qsTr("Scenarios")
                Accessible.name: "Scenarios"
            }
            TabButton {
                objectName: "benchmarksTab"
                text: qsTr("Benchmarks")
                Accessible.name: "Benchmarks"
            }
        }

        StackLayout {
            id: workspaceStack
            objectName: "workspaceStack"
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: workspaceTabBar.currentIndex

            ScenarioWorkspace {
                objectName: "scenarioWorkspace"
                graphVm: root.graphVm
                playtimeVm: root.playtimeVm
                historyVm: root.historyVm
                scenarioBrowserVm: root.scenarioBrowserVm
                visualSettings: visualSettings
                onConfigureGraphLinesRequested: settingsDialog.openGraphLines()
            }

            BenchmarkWorkspace {
                objectName: "benchmarkWorkspace"
                trackingVm: root.benchmarkTrackingVm
                onManageBenchmarksRequested: benchmarkManagerDialog.open()
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
