import QtQuick
import QtQuick.Controls

MenuBar {
    id: root

    property var graphVm
    property var historyVm
    property var visualSettings

    readonly property var historyColumns: root.historyVm ? root.historyVm.allSeries : []

    signal setSourceDirRequested()
    signal settingsRequested()
    signal configureGraphLinesRequested()
    signal loadPerformanceFileRequested()
    signal quitRequested()
    signal aboutRequested()

    Menu {
        title: qsTr("&File")
        Action {
            id: setSoruceDirAction
            text: qsTr("Set Source &Directory")
            onTriggered: setSourceDirRequested()
        }
        Action {
            id: loadPerformanceFileAction
            text: qsTr("&Load Performance File...")
            onTriggered: loadPerformanceFileRequested()
        }

        Action {
            id: settingsAction
            text: qsTr("&Settings")
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

        Action {
            text: qsTr("Scenario Graph")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.scenarioGraphVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.scenarioGraphVisible = checked
        }
        Menu {
            objectName: "scenarioGraphLinesMenu"
            title: qsTr("Scenario Graph Lines")
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
            text: qsTr("Playtime Graph")
            checkable: true
            checked: root.visualSettings ? root.visualSettings.playtimeGraphVisible : true
            onTriggered: if (root.visualSettings) root.visualSettings.playtimeGraphVisible = checked
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
                    checked: !!root.visualSettings.historyColumnVisibility[modelData.id]
                    onTriggered: root.visualSettings.historyColumnVisibility[modelData.id] = checked
                }
            }
        }
        Action {
            text: qsTr("Control Panel")
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
        title: qsTr("&Help")
        Action {
            text: qsTr("&About")
            onTriggered: aboutRequested()
        }
    }
}
