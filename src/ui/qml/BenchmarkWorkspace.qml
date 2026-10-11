import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var trackingVm

    signal manageBenchmarksRequested()

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
                            text: entryRow.modelData.name + "  —  " + entryRow.modelData.classification
                            onClicked: {
                                if (entryRow.rowSelectable && String(entryRow.modelData.id).length > 0)
                                    root.trackingVm.selectBenchmark(entryRow.modelData.id)
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
        ScrollView {
            id: incompleteScroll
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical: ListScrollBar {
                // A ScrollView leaves a replacement bar's geometry to the caller.
                parent: incompleteScroll
                x: incompleteScroll.width - width
                y: incompleteScroll.topPadding
                height: incompleteScroll.availableHeight
                overflowing: incompleteScroll.contentHeight > incompleteScroll.height + 1
            }

            ColumnLayout {
                width: root._contentWidth
                spacing: 8

                Label {
                    objectName: "incompleteBody"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("This benchmark is not ready to rank. Finish its setup to unlock ranks.")
                }

                Repeater {
                    model: (root.trackingVm && root.trackingVm.completenessIssues)
                           ? root.trackingVm.completenessIssues : []
                    delegate: Label {
                        required property var modelData
                        required property int index
                        objectName: "completenessIssue_" + index
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "• " + modelData
                    }
                }

                Button {
                    objectName: "editBenchmarkButton"
                    text: qsTr("Edit benchmark")
                    onClicked: root.manageBenchmarksRequested()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: qsTr("Total playtime")
                        color: root.palette.placeholderText
                    }
                    Label {
                        objectName: "incompleteTotalPlaytime"
                        text: root.trackingVm ? root.trackingVm.totalPlaytime : ""
                        font.bold: true
                    }
                }

                BenchmarkHistoryCard {
                    objectName: "playtimeHistoryCard"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 200
                    namePrefix: "playtimeHistory"
                    historyModel: root.trackingVm ? root.trackingVm.playtimeHistory : null
                }

                BenchmarkProgressGrid {
                    objectName: "benchmarkProgressGrid"
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    grid: root.trackingVm ? root.trackingVm.progressGrid : null
                }
            }
        }
    }

    Component {
        id: trackableComponent
        ScrollView {
            id: benchmarkWorkspaceScroll
            objectName: "benchmarkWorkspaceScroll"
            contentWidth: availableWidth
            clip: true
            // Always visible while the content overflows: the history cards usually
            // starts below the fold, and the style's own bar only shows mid-scroll.
            ScrollBar.vertical: ListScrollBar {
                // A ScrollView leaves a replacement bar's geometry to the caller.
                parent: benchmarkWorkspaceScroll
                x: benchmarkWorkspaceScroll.width - width
                y: benchmarkWorkspaceScroll.topPadding
                height: benchmarkWorkspaceScroll.availableHeight
                overflowing: benchmarkWorkspaceScroll.contentHeight > benchmarkWorkspaceScroll.height + 1
            }

            // Explicit x/y placement rather than a Flow: the two history cards
            // stay direct children (the scroll-order test walks them by index) yet
            // their side-by-side / stacked geometry resolves without a layout
            // polish, which the responsive tests rely on.
            Item {
                id: trackableScrollColumn
                objectName: "trackableScrollColumn"
                width: root._contentWidth
                implicitWidth: width
                implicitHeight: root._wide ? rankCard.y + Math.max(rankCard.height, playtimeCard.height)
                                           : playtimeCard.y + playtimeCard.height

                readonly property real gap: 12
                readonly property real halfWidth: Math.floor((width - gap) / 2)

                BenchmarkStatusSummary {
                    id: summaryBlock
                    objectName: "benchmarkStatusSummary"
                    x: 0
                    y: 0
                    width: trackableScrollColumn.width
                    trackingVm: root.trackingVm
                }

                ColumnLayout {
                    id: progressBlock
                    x: 0
                    y: summaryBlock.y + summaryBlock.height + trackableScrollColumn.gap
                    width: trackableScrollColumn.width
                    spacing: 4

                    Label {
                        objectName: "nextTierProgress"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        textFormat: Text.StyledText
                        text: {
                            if (!root.trackingVm) return ""
                            var total = root.trackingVm.scenarioCount
                            if (total === undefined || total === null) total = "?"
                            var line = qsTr("%1 of %2 scenarios at the next tier")
                                       .arg(root.trackingVm.scenariosAtNextTier).arg(total)
                            var color = String(root.trackingVm.nextTierColor || "")
                            if (color.length === 0) return line
                            return line + " (<font color=\"" + color + "\">"
                                   + String(root.trackingVm.nextTier).replace(/&/g, "&amp;")
                                         .replace(/</g, "&lt;").replace(/>/g, "&gt;")
                                   + "</font>)"
                        }
                    }
                    // Every scenario with its tier bar; rows that block the next tier
                    // are marked, so the count above has its reasons right below it.
                    BenchmarkProgressGrid {
                        objectName: "benchmarkProgressGrid"
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        Layout.topMargin: 4
                        grid: root.trackingVm ? root.trackingVm.progressGrid : null
                    }
                }

                BenchmarkHistoryCard {
                    id: rankCard
                    objectName: "rankHistoryCard"
                    x: 0
                    y: progressBlock.y + progressBlock.height + trackableScrollColumn.gap
                    width: root._wide ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
                    height: Math.max(implicitHeight, 220)
                    namePrefix: "rankHistory"
                    historyModel: root.trackingVm ? root.trackingVm.rankHistory : null
                }

                BenchmarkHistoryCard {
                    id: playtimeCard
                    objectName: "playtimeHistoryCard"
                    x: root._wide ? (trackableScrollColumn.halfWidth + trackableScrollColumn.gap) : 0
                    y: root._wide ? rankCard.y
                                  : (rankCard.y + rankCard.height + trackableScrollColumn.gap)
                    width: root._wide ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
                    height: Math.max(implicitHeight, 220)
                    namePrefix: "playtimeHistory"
                    historyModel: root.trackingVm ? root.trackingVm.playtimeHistory : null
                }
            }
        }
    }
}
