import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import KovaaksStatsViewer

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
            // While the Scenarios workspace shows its full first-run welcome the
            // banner would only repeat it (and would shift the tab bar when switching
            // tabs); it remains for a folder-less setup that still has runs to show.
            visible: !root.settingsVm.kovaaksDirSet && !scenarioWorkspace.welcomeVisible
            onChooseFolderRequested: folderDialog.open()
        }

        // A workspace switcher styled as the app's primary navigation: tall tabs,
        // the active one raised to the content colour with an accent underline, the
        // inactive one dimmed. Sized by content (not stretched across the window) so
        // the bar reads as a set of destinations rather than a table header.
        component WorkspaceTabButton: TabButton {
            id: tab
            readonly property int minimumTabWidth: 150

            implicitHeight: 42
            leftPadding: 24
            rightPadding: 24
            // An explicit width stops TabBar from stretching the tabs to fill the window.
            width: Math.max(minimumTabWidth, implicitContentWidth + leftPadding + rightPadding)

            background: Rectangle {
                color: tab.checked
                       ? tab.palette.window
                       : (tab.hovered ? Qt.alpha(tab.palette.text, 0.08) : "transparent")

                Rectangle {
                    // Accent underline marks the active workspace.
                    visible: tab.checked
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 3
                    color: tab.palette.accent
                }
                Rectangle {
                    // Keyboard focus ring; the bar takes focus for Left/Right switching.
                    visible: tab.visualFocus
                    anchors.fill: parent
                    anchors.margins: 2
                    color: "transparent"
                    border.width: 1
                    border.color: tab.palette.accent
                }
            }
            contentItem: Label {
                text: tab.text
                font.pixelSize: 15
                font.bold: tab.checked
                color: tab.checked ? tab.palette.text : Qt.alpha(tab.palette.text, 0.62)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        TabBar {
            id: workspaceTabBar
            objectName: "workspaceTabBar"
            Layout.fillWidth: true
            focusPolicy: Qt.StrongFocus
            background: Rectangle {
                color: Qt.darker(workspaceTabBar.palette.window, 1.3)

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: workspaceTabBar.palette.mid
                }
            }
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

            WorkspaceTabButton {
                objectName: "scenariosTab"
                text: qsTr("Scenarios")
                Accessible.name: "Scenarios"
            }
            WorkspaceTabButton {
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
                id: scenarioWorkspace
                objectName: "scenarioWorkspace"
                kovaaksFolderSet: root.settingsVm.kovaaksDirSet
                onChooseKovaaksFolderRequested: folderDialog.open()
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
                onEditBenchmarkRequested: (filename, benchmarkId) =>
                    benchmarkManagerDialog.openOnEntry(filename, benchmarkId)
            }
        }
    }

    // Ctrl+F searches scenarios from either workspace. A window shortcut: Settings and the
    // Benchmark Manager are their own windows, so it never takes keys from their text fields.
    Shortcut {
        objectName: "findScenarioShortcut"
        sequence: "Ctrl+F"
        onActivated: {
            workspaceTabBar.currentIndex = 0
            scenarioWorkspace.focusScenarioSearch()
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
