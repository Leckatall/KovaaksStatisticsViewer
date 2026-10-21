import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

MenuBar {
    id: root

    property var graphVm
    property var historyVm
    property var visualSettings
    property bool profileImportBusy: false
    property bool profileRefreshAvailable: false

    readonly property var historyColumns: root.historyVm ? root.historyVm.allSeries : []

    signal setSourceDirRequested()
    signal settingsRequested()
    signal configureGraphLinesRequested()
    signal loadPerformanceFileRequested()
    signal refreshProfileRequested()
    signal manageBenchmarksRequested()
    signal quitRequested()
    signal aboutRequested()

    // An item that keeps the check box's column even when it has no check box, so in a menu that
    // mixes checkable items with submenus and plain actions every label starts at the same x.
    component AlignedMenuItem: MenuItem {
        leftPadding: checkable ? padding : padding + (indicator ? indicator.width : 0) + spacing
    }

    Menu {
        title: qsTr("&File")
        Action {
            id: setSoruceDirAction
            text: qsTr("Set Source &Directory...")
            onTriggered: setSourceDirRequested()
        }
        Action {
            id: loadPerformanceFileAction
            text: qsTr("&Load Performance File...")
            shortcut: "Ctrl+O"
            onTriggered: loadPerformanceFileRequested()
        }

        Action {
            objectName: "refreshProfileAction"
            text: qsTr("&Refresh Profile")
            shortcut: "F5"
            enabled: root.profileRefreshAvailable && !root.profileImportBusy
            onTriggered: root.refreshProfileRequested()
        }

        Action {
            id: settingsAction
            text: qsTr("&Settings...")
            shortcut: "Ctrl+,"
            onTriggered: settingsRequested()
        }
        MenuSeparator {
        }
        Action {
            text: qsTr("&Quit")
            onTriggered: quitRequested()
        }
    }
    Menu {
        title: qsTr("&View")
        delegate: AlignedMenuItem {}

        Action {
            text: qsTr("Scenario Graph")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.scenarioGraphVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.scenarioGraphVisible = checked
        }
        Menu {
            objectName: "scenarioGraphLinesMenu"
            title: qsTr("Scenario Graph Lines")
            delegate: AlignedMenuItem {}
            enabled: root.visualSettings ? root.visualSettings.scenarioGraphVisible : true
            Repeater {
                model: root.graphVm ? root.graphVm.allSeries : []
                MenuItem {
                    required property var modelData
                    text: modelData.name
                    checkable: true
                    checked: root.visualSettings.isSeriesVisible(modelData.id)
                    onTriggered: root.visualSettings.setSeriesVisible(modelData.id, checked)
                }
            }
            MenuSeparator {}
            Action {
                objectName: "configureGraphLinesMenuItem"
                text: qsTr("Configure Lines...")
                onTriggered: root.configureGraphLinesRequested()
            }
        }
        Action {
            text: qsTr("Scenario History")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.scenarioHistoryGraphVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.scenarioHistoryGraphVisible = checked
        }
        Menu {
            objectName: "scenarioHistoryLinesMenu"
            title: qsTr("Scenario History Lines")
            enabled: root.visualSettings ? root.visualSettings.scenarioHistoryGraphVisible : true
            Repeater {
                model: root.historyColumns
                MenuItem {
                    required property var modelData
                    text: modelData.name
                    checkable: true
                    checked: root.visualSettings.isHistorySeriesVisible(modelData.id)
                    onTriggered: root.visualSettings.setHistorySeriesVisible(modelData.id, checked)
                }
            }
        }
        // The graph entries follow the panels' top-to-bottom order.
        Action {
            text: qsTr("Playtime Graph")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.playtimeGraphVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.playtimeGraphVisible = checked
        }
        Action {
            // The line toggles now sit inside the run graph panel (formerly a separate "Control Panel").
            text: qsTr("Graph Line Toggles")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.controlPanelVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.controlPanelVisible = checked
        }
        Action {
            text: qsTr("Selection Panel")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.selectionPanelVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.selectionPanelVisible = checked
        }
        Menu {
            objectName: "selectionPanelSectionsMenu"
            title: qsTr("Selection Panel Sections")
            enabled: root.visualSettings ? root.visualSettings.selectionPanelVisible : true
            MenuItem {
                text: qsTr("Recent Runs")
                checkable: true
                checked: root.visualSettings ? root.visualSettings.recentRunsSectionVisible : true
                onTriggered: if (root.visualSettings) root.visualSettings.recentRunsSectionVisible = checked
            }
            MenuItem {
                text: qsTr("Scenario Browser")
                checkable: true
                checked: root.visualSettings ? root.visualSettings.scenarioBrowserSectionVisible : true
                onTriggered: if (root.visualSettings) root.visualSettings.scenarioBrowserSectionVisible = checked
            }
        }
    }
    Menu {
        title: qsTr("&Benchmarks")
        Action {
            objectName: "manageBenchmarksAction"
            text: qsTr("&Manage Benchmarks...")
            onTriggered: root.manageBenchmarksRequested()
        }
    }
    // Help stays last, as Windows menu bars put it.
    Menu {
        title: qsTr("&Help")
        Action {
            text: qsTr("&About")
            onTriggered: aboutRequested()
        }
    }
}
