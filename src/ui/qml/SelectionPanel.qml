import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    property var scenarioModel
    property var runModel
    property var recentRunModel
    property string currentRunHash
    property double currentRunStartTimeMs: 0
    property alias searchText: scenarioSearch.searchText
    // The active scenario comes from the view model (it follows the current run);
    // the panel only reports clicks through scenarioActivated.
    property string activeScenarioHash
    property string activeScenarioName
    property bool recentExpanded: false
    property bool recentSectionVisible: true
    property bool scenarioBrowserSectionVisible: true
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

    // Recent is bounded to whole rows (never a row cut through its text) and to
    // about a quarter of the panel, so the scenario list and run list below always
    // keep room; anything beyond that scrolls, with an always-visible scroll bar.
    readonly property int recentMaximumRows: Math.max(3, Math.min(6, Math.floor(root.height * 0.25 / recentRuns.rowHeight)))
    readonly property int recentVisibleRows: Math.max(1, Math.min(recentRuns.runCount, recentMaximumRows))

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

    Button {
        objectName: "recentToggleButton"
        Layout.fillWidth: true
        visible: root.recentSectionVisible
        text: (root.recentExpanded ? "▾ " : "▸ ") + qsTr("Recent")
              + (recentRuns.runCount > 0 ? "  (" + recentRuns.runCount + ")" : "")
        Accessible.name: root.recentExpanded ? qsTr("Collapse recent runs") : qsTr("Expand recent runs")
        onClicked: root.recentExpanded = !root.recentExpanded
    }

    RunListView {
        id: recentRuns
        objectName: "recentRunsView"
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.preferredHeight: recentRuns.heightForRows(root.recentVisibleRows)
        Layout.maximumHeight: recentRuns.heightForRows(root.recentVisibleRows)
        visible: root.recentExpanded && root.recentSectionVisible
        title: ""
        showSort: false
        showScenarioName: true
        emptyText: qsTr("No runs yet.")
        runModel: root.recentRunModel
        currentRunHash: root.currentRunHash
        currentRunStartTimeMs: root.currentRunStartTimeMs
        onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
    }

    ScenarioSearchPanel {
        id: scenarioSearch
        objectName: "scenarioSearchPanel"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.verticalStretchFactor: 2
        Layout.minimumHeight: Math.min(160, scenarioSearch.fittedHeight)
        Layout.maximumHeight: root.activeScenarioName !== "" ? Math.max(Layout.minimumHeight, scenarioSearch.fittedHeight)
                                                             : Number.POSITIVE_INFINITY
        Layout.preferredHeight: 0
        visible: root.scenarioBrowserSectionVisible
        scenarioModel: root.scenarioModel
        currentScenarioHash: root.activeScenarioHash
        onSearchEdited: text => root.searchEdited(text)
        onScenarioActivated: (hash, name) => root.scenarioActivated(hash, name)
        onScenarioSortRequested: (field, ascending) => root.scenarioSortRequested(field, ascending)
    }

    RunListView {
        id: scenarioRuns
        objectName: "scenarioRunsView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.verticalStretchFactor: 3
        Layout.minimumHeight: scenarioRuns.heightForRows(3)
        Layout.preferredHeight: 0
        title: root.activeScenarioName
        visible: root.activeScenarioName !== "" && root.scenarioBrowserSectionVisible
        runModel: root.runModel
        currentRunHash: root.currentRunHash
        currentRunStartTimeMs: root.currentRunStartTimeMs
        onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
        onSortRequested: (field, ascending) => root.sortRequested(field, ascending)
    }
}
