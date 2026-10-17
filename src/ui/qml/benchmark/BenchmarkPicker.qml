import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// The current benchmark's name with a chevron, opening a searchable list of every benchmark file.
// Signal-out only: it is handed the entries and the selected name and holds no view model.
Item {
    id: root

    // Entries as BenchmarkTrackingViewModel::selectorEntries publishes them, already in display order.
    property var entries: []
    property string selectedName: ""

    readonly property alias popup: pickerPopup

    signal benchmarkSelected(string id)
    signal manageBenchmarksRequested()
    // A problem file may have no id, so its filename identifies it; the id is passed for entries that
    // have one.
    signal editBenchmarkRequested(string filename, string benchmarkId)

    implicitWidth: selector.implicitWidth
    implicitHeight: selector.implicitHeight

    readonly property string _query: searchField.text.toLowerCase()
    readonly property var _filtered: (root.entries || []).filter(
        entry => String(entry.name).toLowerCase().includes(root._query))

    // Index into _filtered of the row Enter would select; -1 when no visible row is selectable.
    property int _highlight: -1

    function _selectable(entry) {
        return entry.selectable === true && String(entry.id).length > 0
    }

    function _resetHighlight() {
        const list = root._filtered
        let index = list.findIndex(entry => root._selectable(entry) && entry.name === root.selectedName)
        if (index < 0)
            index = list.findIndex(entry => root._selectable(entry))
        root._highlight = index
    }

    function _moveHighlight(step) {
        const list = root._filtered
        for (let i = root._highlight + step; i >= 0 && i < list.length; i += step) {
            if (root._selectable(list[i])) {
                root._highlight = i
                return
            }
        }
    }

    function _choose(entry) {
        if (!root._selectable(entry))
            return
        pickerPopup.close()
        root.benchmarkSelected(String(entry.id))
    }

    function _requestManage() {
        pickerPopup.close()
        root.manageBenchmarksRequested()
    }

    on_FilteredChanged: _resetHighlight()
    on_HighlightChanged: if (_highlight >= 0) entryList.positionViewAtIndex(_highlight, ListView.Contain)

    AbstractButton {
        id: selector
        objectName: "benchmarkSelector"
        anchors.fill: parent
        padding: 2
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.ComboBox
        Accessible.name: qsTr("Benchmark selector")
        onClicked: pickerPopup.opened ? pickerPopup.close() : pickerPopup.open()

        background: Rectangle {
            // The underline is the only focus cue: the control is otherwise chrome-free.
            visible: selector.visualFocus
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: root.palette.highlight
        }

        contentItem: RowLayout {
            spacing: 6

            Label {
                objectName: "benchmarkSelectorTitle"
                Layout.fillWidth: true
                text: root.selectedName.length > 0 ? root.selectedName : qsTr("Select a benchmark")
                font.bold: true
                elide: Text.ElideRight
                color: root.selectedName.length > 0 ? root.palette.windowText : root.palette.placeholderText
            }

            // Two arms meeting at the bottom centre; flips to point up while the popup is open.
            Item {
                id: chevron
                objectName: "benchmarkSelectorChevron"
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
                rotation: pickerPopup.opened ? 180 : 0

                Repeater {
                    model: [45, -45]
                    delegate: Rectangle {
                        required property int modelData
                        width: 6
                        height: 1.5
                        radius: height / 2
                        x: (modelData > 0 ? 4 : 8) - width / 2
                        y: 6 - height / 2
                        rotation: modelData
                        color: root.palette.placeholderText
                    }
                }
            }
        }
    }

    Popup {
        id: pickerPopup
        objectName: "benchmarkSelectorPopup"

        parent: selector
        x: 0
        y: selector.height + 4
        width: Math.max(320, selector.width)
        padding: 6
        margins: 8
        popupType: Popup.Item
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        onAboutToShow: {
            searchField.text = ""
            root._resetHighlight()
        }
        onOpened: searchField.forceActiveFocus()
        onClosed: selector.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: 4

            TextField {
                id: searchField
                objectName: "benchmarkSearchField"
                Layout.fillWidth: true
                placeholderText: qsTr("Search benchmarks")
                selectByMouse: true
                Accessible.name: qsTr("Search benchmarks")

                Keys.onPressed: event => {
                    switch (event.key) {
                    case Qt.Key_Down:
                        root._moveHighlight(1)
                        break
                    case Qt.Key_Up:
                        root._moveHighlight(-1)
                        break
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (root._highlight >= 0)
                            root._choose(root._filtered[root._highlight])
                        break
                    default:
                        // Keys.onPressed accepts by default, which would swallow typing.
                        event.accepted = false
                        return
                    }
                    event.accepted = true
                }
            }

            ListView {
                id: entryList
                objectName: "benchmarkEntryList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 280)
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root._filtered

                delegate: Item {
                    id: entryRow
                    required property var modelData
                    required property int index
                    readonly property bool rowSelectable: root._selectable(modelData)
                    readonly property bool isSelected: modelData.name === root.selectedName
                    readonly property string kind: String(modelData.classification).toLowerCase()
                    // Every exception (incomplete, invalid, unsupported) carries a chip and a way to
                    // the manager, where its details are shown.
                    readonly property bool hasWarning: kind !== "trackable"
                    width: ListView.view.width
                    implicitHeight: entryDelegate.implicitHeight
                    height: entryDelegate.implicitHeight

                    ItemDelegate {
                        id: entryDelegate
                        objectName: "selectorEntry_" + entryRow.modelData.name
                        anchors.fill: parent
                        enabled: entryRow.rowSelectable
                        focusPolicy: Qt.NoFocus
                        highlighted: entryRow.index === root._highlight
                        text: entryRow.modelData.name
                        leftPadding: 28
                        rightPadding: entryRow.hasWarning
                                      ? entryChip.width + entryEditButton.width + 24 : padding
                        onHoveredChanged: if (hovered && entryRow.rowSelectable) root._highlight = entryRow.index
                        onClicked: root._choose(entryRow.modelData)
                    }

                    Label {
                        objectName: "selectorEntryCheck_" + entryRow.modelData.name
                        visible: entryRow.isSelected
                        x: 9
                        anchors.verticalCenter: parent.verticalCenter
                        text: "✓"
                        font.bold: true
                        color: entryDelegate.highlighted ? root.palette.highlightedText : root.palette.text
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

                    // A disabled Invalid/Unsupported row must not pass a click through to the row
                    // stacked under it.
                    MouseArea {
                        anchors.fill: parent
                        enabled: !entryRow.rowSelectable
                        onPressed: mouse => mouse.accepted = true
                    }

                    // Declared after the click blocker so it stays clickable on rows that cannot be
                    // selected; it opens the manager without selecting the row.
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
                        // A loaded (incomplete) benchmark opens in the editor; a problem file can only
                        // be shown in the library.
                        text: entryRow.kind === "incomplete"
                              ? qsTr("Edit %1 in the Benchmark Manager").arg(entryRow.modelData.name)
                              : qsTr("Show %1 in the Benchmark Manager").arg(entryRow.modelData.name)
                        Accessible.name: text
                        ToolTip.text: text
                        onClicked: {
                            pickerPopup.close()
                            root.editBenchmarkRequested(String(entryRow.modelData.filename || ""),
                                                        String(entryRow.modelData.id || ""))
                        }
                    }
                }
            }

            Label {
                objectName: "selectorNoMatches"
                Layout.fillWidth: true
                Layout.margins: 6
                visible: root._filtered.length === 0
                text: qsTr("No benchmarks match")
                color: root.palette.placeholderText
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.alpha(root.palette.text, 0.2)
            }

            ItemDelegate {
                objectName: "selectorManageBenchmarks"
                Layout.fillWidth: true
                text: qsTr("Manage Benchmarks…")
                onClicked: root._requestManage()
                Keys.onReturnPressed: root._requestManage()
                Keys.onEnterPressed: root._requestManage()
            }
        }
    }
}
