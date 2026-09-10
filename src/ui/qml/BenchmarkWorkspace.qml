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

    // Widths in the trackable body derive from this instead of a ScrollView's
    // availableWidth: `width` is assigned directly and takes effect before the
    // first layout polish, so responsive geometry is correct on the same frame.
    readonly property real _contentWidth: Math.max(0, width - 24)

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

    // --- always-visible header ----------------------------------------------
    RowLayout {
        id: headerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 12

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
                Label {
                    visible: !root._hasAnyEntry
                    text: qsTr("No benchmark files found.")
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

        Label {
            objectName: "benchmarkClassificationLabel"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: {
                if (!root.trackingVm) return ""
                var name = root.trackingVm.selectedName
                var msg = root.trackingVm.stateMessage
                return name && String(name).length > 0 ? name + " — " + msg : msg
            }
        }

        Button {
            objectName: "manageBenchmarksButton"
            text: qsTr("Manage Benchmarks")
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
        anchors.top: headerRow.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 8
        sourceComponent: root._bodyComponent
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
        ColumnLayout {
            objectName: "emptyLibraryBody"
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("No benchmarks are installed yet.")
            }
            Button {
                objectName: "createOrImportButton"
                text: qsTr("Create or import a benchmark")
                onClicked: root.manageBenchmarksRequested()
            }
            Item { Layout.fillHeight: true }
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
            contentWidth: availableWidth
            clip: true

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

                BenchmarkBreakdownView {
                    objectName: "benchmarkBreakdownView"
                    Layout.fillWidth: true
                    breakdown: root.trackingVm ? root.trackingVm.breakdown : null
                    trackingVm: root.trackingVm
                    wide: root._wide
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

            // Explicit x/y placement rather than a Flow: the two history cards
            // stay direct children (the scroll-order test walks them by index) yet
            // their side-by-side / stacked geometry resolves without a layout
            // polish, which the responsive tests rely on.
            Item {
                id: trackableScrollColumn
                objectName: "trackableScrollColumn"
                width: root._contentWidth
                implicitWidth: width
                implicitHeight: breakdownView.y + breakdownView.height

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
                        text: {
                            if (!root.trackingVm) return ""
                            var total = root.trackingVm.scenarioCount
                            if (total === undefined || total === null) total = "?"
                            return qsTr("%1 of %2 scenarios at the next tier")
                                   .arg(root.trackingVm.scenariosAtNextTier).arg(total)
                        }
                    }
                    BenchmarkBlockerList {
                        objectName: "benchmarkBlockerList"
                        Layout.fillWidth: true
                        blockers: (root.trackingVm && root.trackingVm.blockers)
                                  ? root.trackingVm.blockers : []
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

                BenchmarkBreakdownView {
                    id: breakdownView
                    objectName: "benchmarkBreakdownView"
                    x: 0
                    y: (root._wide
                        ? rankCard.y + Math.max(rankCard.height, playtimeCard.height)
                        : playtimeCard.y + playtimeCard.height) + trackableScrollColumn.gap
                    width: trackableScrollColumn.width
                    breakdown: root.trackingVm ? root.trackingVm.breakdown : null
                    trackingVm: root.trackingVm
                    wide: root._wide
                }
            }
        }
    }
}
