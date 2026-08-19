import QtQuick
import QtQuick.Controls

MenuBar {
    id: root

    property var graphVm
    property var historyVm
    property var seriesVisibility
    property var columnVisibility
    readonly property var effectiveVisibility: root.seriesVisibility || root.columnVisibility
    readonly property var graphLineIds: root.graphVm ? (
        root.graphVm.enabledSeriesIds !== undefined ? root.graphVm.enabledSeriesIds
                                                     : root.graphVm.enabledColumns.map(
                                                           c => root.graphVm.columnKey(c))) : []

    function seriesFor(id) {
        if (root.graphVm && root.graphVm.allSeries) {
            const found = root.graphVm.allSeries.find(s => s.id === id)
            if (found) return found
        }
        return null
    }

    function graphLineName(id) {
        const series = seriesFor(id)
        if (series) return series.name
        const column = root.graphVm.allColumns.find(c => root.graphVm.columnKey(c) === id)
        return column === undefined ? id : root.graphVm.columnName(column)
    }
    property var historyColumnVisibility
    property var viewSettings
    readonly property var historyColumns: root.historyVm ? [
        CompletionHistoryViewModel.Score,
        CompletionHistoryViewModel.Accuracy,
        CompletionHistoryViewModel.Shots,
        CompletionHistoryViewModel.Hits,
        CompletionHistoryViewModel.Misses
    ] : []

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

        // Action {
        //     text: qsTr("&Save")
        // }
        // Action {
        //     text: qsTr("Save &As...")
        // }
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
            checked: root.viewSettings ? root.viewSettings.scenarioGraphVisible : true
            onTriggered: if (root.viewSettings) root.viewSettings.scenarioGraphVisible = checked
        }
        Menu {
            objectName: "scenarioGraphLinesMenu"
            title: qsTr("Scenario Graph Lines")
            enabled: root.viewSettings ? root.viewSettings.scenarioGraphVisible : true
            Repeater {
                model: root.graphLineIds
                MenuItem {
                    required property string modelData
                    text: root.graphLineName(modelData)
                    checkable: true
                    checked: SeriesVisibility.read(root.effectiveVisibility, modelData)
                    onTriggered: {
                        if (root.seriesVisibility && root.seriesVisibility.setValue)
                            SeriesVisibility.write(root.seriesVisibility, modelData, checked)
                        else
                            root.columnVisibility[modelData] = checked
                    }
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
            checked: root.viewSettings ? root.viewSettings.playtimeGraphVisible : true
            onTriggered: if (root.viewSettings) root.viewSettings.playtimeGraphVisible = checked
        }
        Action {
            text: qsTr("Scenario History")
            checkable: true
            checked: root.viewSettings ? root.viewSettings.scenarioHistoryGraphVisible : true
            onTriggered: if (root.viewSettings) root.viewSettings.scenarioHistoryGraphVisible = checked
        }
        Menu {
            objectName: "scenarioHistoryLinesMenu"
            title: qsTr("Scenario History Lines")
            enabled: root.viewSettings ? root.viewSettings.scenarioHistoryGraphVisible : true
            Repeater {
                model: root.historyColumns
                MenuItem {
                    required property int modelData
                    text: root.historyVm.columnName(modelData)
                    checkable: true
                    checked: !!root.historyColumnVisibility[root.historyVm.columnKey(modelData)]
                    onTriggered: root.historyColumnVisibility[root.historyVm.columnKey(modelData)] = checked
                }
            }
        }
        Action {
            text: qsTr("Control Panel")
            checkable: true
            checked: root.viewSettings ? root.viewSettings.controlPanelVisible : true
            onTriggered: if (root.viewSettings) root.viewSettings.controlPanelVisible = checked
        }
        Action {
            text: qsTr("Selection Panel")
            checkable: true
            checked: root.viewSettings ? root.viewSettings.selectionPanelVisible : true
            onTriggered: if (root.viewSettings) root.viewSettings.selectionPanelVisible = checked
        }
        Menu {
            objectName: "selectionPanelSectionsMenu"
            title: qsTr("Selection Panel Sections")
            enabled: root.viewSettings ? root.viewSettings.selectionPanelVisible : true
            MenuItem {
                text: qsTr("Recent Runs")
                checkable: true
                checked: root.viewSettings ? root.viewSettings.recentRunsSectionVisible : true
                onTriggered: if (root.viewSettings) root.viewSettings.recentRunsSectionVisible = checked
            }
            MenuItem {
                text: qsTr("Scenario Browser")
                checkable: true
                checked: root.viewSettings ? root.viewSettings.scenarioBrowserSectionVisible : true
                onTriggered: if (root.viewSettings) root.viewSettings.scenarioBrowserSectionVisible = checked
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
