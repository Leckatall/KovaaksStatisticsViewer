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
    property bool recentExpanded: true
    signal searchEdited(string text)
    signal scenarioActivated(string hash, string name)
    signal runSelected(string hash, double startTimeMs)

    ToolButton {
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
    }
}
