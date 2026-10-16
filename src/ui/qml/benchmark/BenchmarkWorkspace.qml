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

    // Bodies that carry their own Benchmark Manager action; while one is shown the
    // header's Manage Benchmarks button would only be a second control for the
    // same thing.
    readonly property bool _bodyOwnsManagerAction: _bodyState === "emptyLibrary"
                                                   || _bodyState === "problemsOnly"
                                                   || _bodyState === "unavailable"
                                                   || _bodyState === "incomplete"

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

    // --- header: selector, status, manager action ---------------------------
    // Hidden with an empty library: there is nothing to select, and the empty-state
    // body carries the one explanation and the one action.
    RowLayout {
        id: headerRow
        objectName: "benchmarkHeader"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 12
        visible: root._hasAnyEntry
        height: visible ? implicitHeight : 0

        Frame {
            id: benchmarkSelector
            objectName: "benchmarkSelector"
            Layout.fillWidth: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Benchmark selector")
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 2

                Label {
                    text: qsTr("Benchmark")
                    font.bold: true
                    color: root.palette.placeholderText
                }
                Repeater {
                    model: root._entries
                    delegate: Item {
                        id: entryRow
                        required property var modelData
                        readonly property bool rowSelectable: modelData.selectable === true
                        readonly property string kind: String(modelData.classification).toLowerCase()
                        // Every exception (incomplete, invalid, unsupported) carries a chip
                        // and a way to the manager, where its details are shown.
                        readonly property bool hasWarning: kind !== "trackable"
                        width: parent.width
                        implicitHeight: entryDelegate.implicitHeight
                        height: entryDelegate.implicitHeight

                        ItemDelegate {
                            id: entryDelegate
                            objectName: "selectorEntry_" + entryRow.modelData.name
                            anchors.fill: parent
                            enabled: entryRow.rowSelectable
                            activeFocusOnTab: entryRow.rowSelectable
                            highlighted: root.trackingVm
                                         && root.trackingVm.selectedName === entryRow.modelData.name
                            text: entryRow.modelData.name
                            // A trackable benchmark is the normal case and shows
                            // only its name; the exceptions get a filled chip.
                            rightPadding: entryRow.hasWarning
                                          ? entryChip.width + entryEditButton.width + 24 : padding
                            onClicked: {
                                if (entryRow.rowSelectable && String(entryRow.modelData.id).length > 0)
                                    root.trackingVm.selectBenchmark(entryRow.modelData.id)
                            }
                        }

                        Rectangle {
                            id: entryChip
                            objectName: "selectorEntryLabel_" + entryRow.modelData.name
                            readonly property string kind: String(entryRow.modelData.classification)
                            visible: entryRow.hasWarning
                            anchors.right: entryEditButton.left
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            width: chipText.implicitWidth + 14
                            height: chipText.implicitHeight + 2
                            radius: height / 2
                            color: entryRow.kind === "incomplete" ? Theme.classificationColor("Incomplete") : Theme.errorText
                            Label {
                                id: chipText
                                anchors.centerIn: parent
                                text: entryChip.kind
                                font.bold: true
                                font.pixelSize: entryDelegate.font.pixelSize - 1
                                color: "#1E1E1E"
                            }
                        }

                        // A disabled Invalid/Unsupported row must never pass a
                        // click through to an enabled row stacked under it before
                        // the Column positioner has run.
                        MouseArea {
                            anchors.fill: parent
                            enabled: !entryRow.rowSelectable
                            onPressed: mouse => mouse.accepted = true
                        }

                        // Declared after the click blocker so it stays clickable on rows that
                        // cannot be selected; it opens the manager without selecting the row.
                        TooltipToolButton {
                            id: entryEditButton
                            objectName: "selectorEntryEdit_" + entryRow.modelData.name
                            visible: entryRow.hasWarning
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: visible ? implicitWidth : 0
                            padding: 4
                            icon.source: "../icons/square-pen.svg"
                            icon.width: 16
                            icon.height: 16
                            display: AbstractButton.IconOnly
                            // A loaded (incomplete) benchmark opens in the editor; a problem
                            // file can only be shown in the library.
                            text: entryRow.kind === "incomplete"
                                  ? qsTr("Edit %1 in the Benchmark Manager").arg(entryRow.modelData.name)
                                  : qsTr("Show %1 in the Benchmark Manager").arg(entryRow.modelData.name)
                            Accessible.name: text
                            ToolTip.text: text
                            onClicked: root.editBenchmarkRequested(String(entryRow.modelData.filename || ""),
                                                                   String(entryRow.modelData.id || ""))
                        }
                    }
                }
            }
        }

        // Every other state explains itself in its body, so a status line here
        // would repeat it; only the tracking state has nothing else to say.
        Label {
            objectName: "benchmarkClassificationLabel"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: root._bodyState === "trackable"
            color: root.palette.placeholderText
            text: root.trackingVm ? String(root.trackingVm.stateMessage || "") : ""
        }
        Item {
            // Keeps the manager button right-aligned while the status line is hidden.
            Layout.fillWidth: true
            visible: root._bodyState !== "trackable"
        }

        Button {
            objectName: "manageBenchmarksButton"
            text: qsTr("Manage Benchmarks")
            visible: !root._bodyOwnsManagerAction
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            onClicked: root.manageBenchmarksRequested()
        }
    }

    // --- state body -------------------------------------------------------
    Loader {
        id: bodyLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerRow.visible ? headerRow.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: headerRow.visible ? 4 : 8
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
