import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    property var scenarioModel
    property var runModel
    property var recentRunModel
    property alias searchText: scenarioSearch.searchText
    property string activeScenarioName
    property bool recentExpanded: false
    // The panel is sized to the widest name in the whole scenario catalogue, not to
    // whatever happens to be listed or selected right now, so clicking around never
    // resizes it. Both inputs come from Main.qml.
    property string widestScenarioName
    property real maximumPanelWidth: 480

    readonly property real chromeWidth: scenarioSearch.implicitWidth
    // Delegate padding, the run-count label beside the name, and scrollbar allowance.
    readonly property real scenarioDelegateChrome: 72

    readonly property real desiredWidth: Math.min(root.maximumPanelWidth,
                                                  Math.max(root.chromeWidth,
                                                           nameMetrics.advanceWidth + root.scenarioDelegateChrome))

    Layout.minimumWidth: root.chromeWidth
    Layout.maximumWidth: root.maximumPanelWidth
    Layout.preferredWidth: root.desiredWidth

    TextMetrics {
        id: nameMetrics
        font: Qt.application.font
        text: root.widestScenarioName
    }

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
        Layout.maximumHeight: root.height / 2
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
