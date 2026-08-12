import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8
    implicitWidth: 360

    property var scenarioModel
    property var runModel
    property var recentRunModel
    property alias searchText: scenarioSearch.searchText
    property string activeScenarioName
    property bool recentExpanded: false
    signal searchEdited(string text)
    signal scenarioActivated(string hash, string name)
    signal runSelected(string hash, double startTimeMs)
    signal sortRequested(int field, bool ascending)
    signal scenarioSortRequested(int field, bool ascending)

    ToolButton {
        objectName: "recentToggleButton"
        Layout.fillWidth: true
        text: (root.recentExpanded ? "▾" : "▸") + " Recent"
        onClicked: root.recentExpanded = !root.recentExpanded
    }

    RunListView {
        id: recentRuns
        objectName: "recentRunsView"
        Layout.fillWidth: true
        Layout.preferredHeight: root.recentExpanded ? 180 : 0
        visible: root.recentExpanded
        title: ""
        showSort: false
        runModel: root.recentRunModel
        onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
    }

    ScenarioSearchPanel {
        id: scenarioSearch
        Layout.fillWidth: true
        Layout.fillHeight: root.activeScenarioName === ""
        Layout.preferredHeight: root.activeScenarioName === "" ? 0 : 180
        scenarioModel: root.scenarioModel
        onSearchEdited: text => root.searchEdited(text)
        onScenarioActivated: (hash, name) => {
            root.activeScenarioName = name
            root.scenarioActivated(hash, name)
        }
        onScenarioSortRequested: (field, ascending) => root.scenarioSortRequested(field, ascending)
    }

    RunListView {
        id: scenarioRuns
        objectName: "scenarioRunsView"
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 220 : 0
        title: root.activeScenarioName
        visible: root.activeScenarioName !== ""
        runModel: root.runModel
        onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
        onSortRequested: (field, ascending) => root.sortRequested(field, ascending)
    }
}
