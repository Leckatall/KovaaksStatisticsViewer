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
    // Recent lists runs of every scenario, so it adds a scenario column.
    property bool showScenarioName: false

    readonly property int rowHeight: Math.ceil(dateProbe.implicitHeight) + 8
    readonly property int runCount: !runModel ? 0
                                    : (runModel.count !== undefined ? runModel.count : runModel.length)
    // Height of the header rows above the list, so a parent can size the list to
    // a whole number of rows (SelectionPanel bounds Recent this way).
    readonly property real chromeHeight: (headerRow.visible ? headerRow.implicitHeight + spacing : 0)
                                         + columnHeader.implicitHeight + spacing
    readonly property bool scrollable: runList.contentHeight > runList.height + 1

    signal runSelected(string hash, double startTimeMs)
    signal sortRequested(int field, bool ascending)

    function hasNoRuns() {
        return runCount === 0
    }

    // Height that shows exactly `rows` rows (no row cut through its text).
    function heightForRows(rows) {
        return chromeHeight + Math.max(1, rows) * rowHeight
    }

    // Column widths are measured with the same (default) Label font the rows use.
    readonly property real scoreColumnWidth: Math.ceil(scoreProbe.implicitWidth) + 2
    readonly property real accuracyColumnWidth: Math.ceil(accuracyProbe.implicitWidth) + 2
    readonly property real personalBestColumnWidth: 28
    readonly property real dateColumnWidth: Math.ceil(dateProbe.implicitWidth) + 2

    Label { id: dateProbe; visible: false; text: "May 30, 23:59" }
    Label { id: scoreProbe; visible: false; text: "000000" }
    Label { id: accuracyProbe; visible: false; text: "100.0%" }

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

    // Column titles, aligned with the cells of ScenarioRunItem.
    RowLayout {
        id: columnHeader
        objectName: "runListColumnHeader"
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 6 + (runList.ScrollBar.vertical.visible ? runList.ScrollBar.vertical.width : 0)
        spacing: 8
        visible: !root.hasNoRuns()
        opacity: 0.6

        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            visible: root.showScenarioName
            font.pixelSize: 11
            text: qsTr("Scenario")
        }
        Label {
            Layout.fillWidth: !root.showScenarioName
            Layout.preferredWidth: root.showScenarioName ? root.dateColumnWidth : 0
            font.pixelSize: 11
            text: qsTr("Played")
        }
        Label {
            Layout.preferredWidth: root.scoreColumnWidth
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 11
            text: qsTr("Score")
        }
        Label {
            Layout.preferredWidth: root.accuracyColumnWidth
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 11
            text: qsTr("Acc")
        }
        Label {
            Layout.preferredWidth: root.personalBestColumnWidth
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11
            text: qsTr("PB")
        }
    }

    ListView {
        id: runList
        objectName: "runListView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: root.rowHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.runModel

        // Always shown while there is more than fits, so a list cut at its last
        // whole row still reads as scrollable.
        ScrollBar.vertical: ListScrollBar {
            objectName: "runListScrollBar"
            overflowing: root.scrollable
        }

        delegate: ScenarioRunItem {
            width: runList.width - (runList.ScrollBar.vertical.visible ? runList.ScrollBar.vertical.width : 0)
            rowHeight: root.rowHeight
            showScenarioName: root.showScenarioName
            dateColumnWidth: root.dateColumnWidth
            scoreColumnWidth: root.scoreColumnWidth
            accuracyColumnWidth: root.accuracyColumnWidth
            personalBestColumnWidth: root.personalBestColumnWidth
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
