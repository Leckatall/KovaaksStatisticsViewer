import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 4

    property var runModel
    property alias title: titleLabel.text
    property string emptyText: qsTr("No runs")
    property string currentRunHash
    property double currentRunStartTimeMs: 0
    property bool showSort: true
    property int sortField: 0
    property bool sortAscending: false
    // Spacing between run cards.
    readonly property int cardSpacing: 6
    // One card plus the gap after it, measured from a hidden card so it always matches the
    // delegate (fonts, padding, PB badge).
    readonly property int rowHeight: Math.ceil(cardProbe.implicitHeight) + cardSpacing
    readonly property int runCount: !runModel ? 0
                                    : (runModel.count !== undefined ? runModel.count : runModel.length)
    // Height of the header row above the list, so a parent can size the list to a
    // whole number of cards (SelectionPanel bounds Recent this way).
    readonly property real chromeHeight: headerRow.visible ? headerRow.implicitHeight + spacing : 0
    readonly property bool scrollable: runList.contentHeight > runList.height + 1

    signal runSelected(string hash, double startTimeMs)
    signal sortRequested(int field, bool ascending)

    function hasNoRuns() {
        return runCount === 0
    }

    // The list's current item follows the selected run, however it was selected (this list,
    // Recent, the history graph), and is scrolled into view.
    function syncCurrentToSelection() {
        const row = (root.runModel && typeof root.runModel.rowOf === "function")
                    ? root.runModel.rowOf(root.currentRunHash, root.currentRunStartTimeMs) : -1
        runList.currentIndex = row
        if (row >= 0) runList.positionViewAtIndex(row, ListView.Contain)
    }
    onCurrentRunHashChanged: Qt.callLater(syncCurrentToSelection)
    onCurrentRunStartTimeMsChanged: Qt.callLater(syncCurrentToSelection)
    onRunCountChanged: Qt.callLater(syncCurrentToSelection)
    Connections {
        target: root.runModel && root.runModel.modelReset !== undefined ? root.runModel : null
        function onModelReset() { Qt.callLater(root.syncCurrentToSelection) }
    }

    // Height that shows exactly `rows` cards (no card cut through its text).
    function heightForRows(rows) {
        return chromeHeight + Math.max(1, rows) * rowHeight - cardSpacing
    }

    ScenarioRunItem {
        id: cardProbe
        // Never shown; index -1 keeps its object names clear of the real runItem_N cards.
        objectName: "runItemProbe"
        visible: false
        index: -1
        hash: ""
        runLabel: "Probe (2026-01-01, 00:00:00)"
        score: 0
        accuracy: 0
        personalBest: true
        startTimeMs: 0
    }

    RowLayout {
        id: headerRow
        Layout.fillWidth: true
        visible: root.title !== "" || root.showSort

        Label {
            id: titleLabel
            font.bold: true
            // preferredWidth 0 keeps the (arbitrarily long) scenario name out of the
            // layout's width hint; it still takes the row's slack via fillWidth.
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 60
            elide: Text.ElideRight
            objectName: "runListTitle"
        }

        Loader {
            // Defer creating the ComboBox/Popup machinery until this view is
            // actually shown; instantiating it while hidden was observed to
            // interfere with unrelated mouse-click delivery elsewhere in the
            // window (e.g. Qt Quick Controls Popup/Overlay setup).
            active: root.showSort && root.visible
            sourceComponent: sortControlsComponent
        }
    }

    Component {
        id: sortControlsComponent

        SortControls {
            comboObjectName: "runSortCombo"
            buttonObjectName: "runSortDirectionButton"
            options: ["Date", "Score", "Accuracy"]
            sortField: root.sortField
            sortAscending: root.sortAscending
            onSortRequested: (field, ascending) => {
                root.sortField = field
                root.sortAscending = ascending
                root.sortRequested(field, ascending)
            }
        }
    }

    ListView {
        id: runList
        objectName: "runListView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: root.rowHeight - root.cardSpacing
        clip: true
        spacing: root.cardSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: root.runModel
        // The selected card draws its own highlight; currentIndex only tracks it (see
        // syncCurrentToSelection), so the view must not animate a second one.
        highlightFollowsCurrentItem: false
        Component.onCompleted: Qt.callLater(root.syncCurrentToSelection)

        // Always shown while there is more than fits, so a list cut at its last
        // whole row still reads as scrollable.
        ScrollBar.vertical: ListScrollBar {
            objectName: "runListScrollBar"
            overflowing: root.scrollable
        }

        delegate: ScenarioRunItem {
            width: runList.width - (runList.ScrollBar.vertical.visible ? runList.ScrollBar.vertical.width + 2 : 0)
            currentRunHash: root.currentRunHash
            currentRunStartTimeMs: root.currentRunStartTimeMs
            onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
        }

        Label {
            objectName: "runListEmptyLabel"
            // Parented to the view itself (not its scrolling content) so it sits at
            // the top of the list area.
            parent: runList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            opacity: 0.7
            visible: root.hasNoRuns()
            text: root.emptyText
        }
    }
}
