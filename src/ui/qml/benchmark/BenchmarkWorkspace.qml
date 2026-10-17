import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Item {
    id: root

    property var trackingVm
    property var settingsVm: null

    signal manageBenchmarksRequested()
    signal moreGraphSettingsRequested(string graphKey)
    // Opens the Benchmark Manager on one selector entry: its draft in the editor when it can be
    // opened, otherwise selected in the library. A problem file may have no id, so its filename
    // identifies it; the id is passed for entries that have one.
    signal editBenchmarkRequested(string filename, string benchmarkId)

    implicitWidth: 800
    implicitHeight: 600

    // The tracking view model publishes one state token per revision. The empty
    // token folds "no files at all" and "only problem files" together, so the
    // body pulls those apart from the selector contents.
    readonly property var _entries: (root.trackingVm && root.trackingVm.selectorEntries)
                                    ? root.trackingVm.selectorEntries : []
    readonly property bool _hasAnyEntry: _entries.length > 0
    readonly property string _rawState: root.trackingVm ? String(root.trackingVm.state || "") : ""
    readonly property string _bodyState: {
        switch (_rawState.toLowerCase()) {
        case "problemsonly":
            return "problemsOnly"
        case "emptylibrary":
            return _hasAnyEntry ? "problemsOnly" : "emptyLibrary"
        case "unavailable":
            return "unavailable"
        case "incomplete":
            return "incomplete"
        case "trackable":
            return "trackable"
        default:
            return "noSelection"
        }
    }
    readonly property bool _wide: width >= 1000

    // Opening the workspace with nothing selected shows a benchmark without an
    // extra click: the use case restores the remembered choice, or picks the only
    // benchmark there is. Re-checked whenever the workspace becomes visible or the
    // library changes while it is visible.
    //
    // Creation-time triggers are deferred: inside the window's StackLayout this item
    // still reports visible while it is being created and is only hidden once the
    // layout runs, so an immediate check would select (and evaluate) a benchmark at
    // startup before the tab is ever opened. Becoming visible (the tab opening) is
    // handled at once, so the first frame of the tab already shows the benchmark.
    function _ensureSelection() {
        if (!root.visible || !root.trackingVm || root._bodyState !== "noSelection") return
        if (typeof root.trackingVm.selectDefaultBenchmark === "function")
            root.trackingVm.selectDefaultBenchmark()
    }
    function _scheduleEnsureSelection() { Qt.callLater(root._ensureSelection) }
    onVisibleChanged: _ensureSelection()
    onTrackingVmChanged: _scheduleEnsureSelection()
    Component.onCompleted: _scheduleEnsureSelection()

    // Widths in the trackable body derive from this instead of a ScrollView's
    // availableWidth: `width` is assigned directly and takes effect before the
    // first layout polish, so responsive geometry is correct on the same frame.
    // 16px of body margins plus room for the always-visible scroll bar.
    readonly property real _contentWidth: Math.max(0, width - 32)

    readonly property Component _bodyComponent: {
        switch (_bodyState) {
        case "emptyLibrary": return emptyLibraryComponent
        case "problemsOnly": return problemsOnlyComponent
        case "unavailable": return unavailableComponent
        case "incomplete": return incompleteComponent
        case "trackable": return trackableComponent
        default: return noSelectionComponent
        }
    }

    // --- pinned card: picker, status line and, while tracking, the summary --
    // Hidden with an empty library: there is nothing to select, and the empty-state
    // body carries the one explanation and the one action. Outside the scrolling
    // body on purpose, and never shrunk for short windows.
    Rectangle {
        id: headerCard
        objectName: "benchmarkHeader"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        visible: root._hasAnyEntry
        height: visible ? cardColumn.implicitHeight + 2 * cardColumn.anchors.margins : 0
        radius: 8
        color: root.palette.base
        border.color: root.palette.mid

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                BenchmarkPicker {
                    Layout.preferredWidth: implicitWidth
                    Layout.maximumWidth: Math.max(160, headerCard.width / 2)
                    entries: root._entries
                    selectedName: root.trackingVm ? String(root.trackingVm.selectedName || "") : ""
                    onBenchmarkSelected: id => root.trackingVm.selectBenchmark(id)
                    onManageBenchmarksRequested: root.manageBenchmarksRequested()
                    onEditBenchmarkRequested: (filename, benchmarkId) =>
                        root.editBenchmarkRequested(filename, benchmarkId)
                }

                // Every other state explains itself in its body, so a status line here
                // would repeat it; only the tracking state has nothing else to say.
                Label {
                    objectName: "benchmarkClassificationLabel"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.WordWrap
                    visible: root._bodyState === "trackable"
                    color: root.palette.placeholderText
                    text: root.trackingVm ? String(root.trackingVm.stateMessage || "") : ""
                }
            }

            // A Loader rather than a hidden item: no summary exists outside the trackable state.
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: item ? item.implicitHeight : 0
                active: root._bodyState === "trackable"
                visible: active
                sourceComponent: BenchmarkStatusSummary {
                    objectName: "benchmarkStatusSummary"
                    width: parent ? parent.width : 0
                    trackingVm: root.trackingVm
                }
            }
        }
    }

    // --- state body -------------------------------------------------------
    Loader {
        id: bodyLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerCard.visible ? headerCard.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: headerCard.visible ? 4 : 8
        anchors.bottomMargin: 8
        sourceComponent: root._bodyComponent
        // A library change can land on "nothing selected" while the tab is open.
        onSourceComponentChanged: root._scheduleEnsureSelection()
    }

    // ======================================================================
    // State bodies
    // ======================================================================

    Component {
        id: noSelectionComponent
        ColumnLayout {
            objectName: "noSelectionBody"
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Select a benchmark above to see its tracking status.")
            }
            Item { Layout.fillHeight: true }
        }
    }

    Component {
        id: emptyLibraryComponent
        // One explanation and one primary action, placed where the eye lands.
        Item {
            objectName: "emptyLibraryBody"

            ColumnLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(0, Math.round(parent.height * 0.22))
                width: Math.min(parent.width - 32, 520)
                spacing: 12

                Label {
                    objectName: "emptyLibraryTitle"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pointSize: emptyLibraryMessage.font.pointSize > 0
                                    ? emptyLibraryMessage.font.pointSize * 1.4 : 13
                    font.bold: true
                    text: qsTr("No benchmarks yet")
                }
                Label {
                    id: emptyLibraryMessage
                    objectName: "emptyLibraryMessage"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: root.palette.placeholderText
                    text: qsTr("A benchmark ranks your scores on a set of scenarios against tier thresholds. "
                               + "Create one, or import a benchmark file, to start tracking.")
                }
                Button {
                    objectName: "createOrImportButton"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    highlighted: true
                    text: qsTr("Create or import a benchmark")
                    onClicked: root.manageBenchmarksRequested()
                }
            }
        }
    }

    Component {
        id: problemsOnlyComponent
        ColumnLayout {
            objectName: "problemsOnlyBody"
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Some benchmark files need attention before they can be tracked.")
            }
            Button {
                objectName: "manageBenchmarkFilesButton"
                text: qsTr("Manage benchmark files")
                onClicked: root.manageBenchmarksRequested()
            }
            Item { Layout.fillHeight: true }
        }
    }

    Component {
        id: unavailableComponent
        ColumnLayout {
            objectName: "unavailableBody"
            spacing: 8
            Label {
                objectName: "unavailableName"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                text: root.trackingVm ? root.trackingVm.selectedName : ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.trackingVm ? root.trackingVm.stateMessage
                                      : qsTr("The selected benchmark is no longer available.")
            }
            Button {
                objectName: "repairButton"
                text: qsTr("Open the Benchmark Manager")
                onClicked: root.manageBenchmarksRequested()
            }
            Item { Layout.fillHeight: true }
        }
    }

    Component {
        id: incompleteComponent
        BenchmarkIncompleteBody {
            trackingVm: root.trackingVm
            settingsVm: root.settingsVm
            bodyWidth: root._contentWidth
            wide: root._wide
            onManageBenchmarksRequested: root.manageBenchmarksRequested()
            onMoreGraphSettingsRequested: key => root.moreGraphSettingsRequested(key)
        }
    }

    Component {
        id: trackableComponent
        BenchmarkTrackableBody {
            trackingVm: root.trackingVm
            settingsVm: root.settingsVm
            bodyWidth: root._contentWidth
            wide: root._wide
            onManageBenchmarksRequested: root.manageBenchmarksRequested()
            onMoreGraphSettingsRequested: key => root.moreGraphSettingsRequested(key)
        }
    }
}
