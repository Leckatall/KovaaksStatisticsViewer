import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Compact benchmark table editor. It renders the manager's tableModel projection and forwards
// commands; the manager owns every value, and delegates only hold the text being typed.
//
// The identifying columns (category, subcategory, scenario) and the threshold columns are two
// TableViews over the same model and selection, synced vertically, so the identifying columns
// stay in place while thresholds scroll sideways. Each view hides the other's columns by giving
// them zero width.
FocusScope {
    id: root

    required property var manager

    // The open cell editor, if any; commitActiveEdit() flushes it.
    property Item activeEditor: null
    readonly property alias identityView: identityView
    readonly property alias thresholdView: thresholdView
    readonly property alias selectionModel: selection
    readonly property var tableModel: root.manager ? root.manager.tableModel : null

    property int rowHeight: 30
    property int headerHeight: 34
    property var identityWidths: [130, 130, 210]
    property int thresholdWidth: 100

    // Context for the dialog's rank, group and row controls: the rank or group last focused (by
    // cell, header or group list), and the current row's entry, spans and mapping.
    property string focusedTierId: ""
    property string focusedGroupId: ""
    property var currentRowInfo: ({})
    // True while a rank header, not a cell, is the paste destination.
    property bool headerFocused: false
    property string statusText: ""
    // Set from a model reset until the visual selection is rebuilt from the manager's anchors: the
    // selection model clears itself on reset, and that must never be published as the user's.
    property bool restoringSelection: false
    // Whether a rank header held focus when the model reset destroyed it.
    property bool restoreHeaderFocus: false

    // `kind` is "tier" or "group"; the dialog owns the colour picker.
    signal colorSwatchRequested(string kind, string id, color current)

    readonly property var kindKeys: ["category", "subcategory", "scenario"]
    readonly property int thresholdKind: 3

    function columnKey(kind, tierId) {
        return kind === root.thresholdKind ? tierId : root.kindKeys[kind]
    }

    function commitCell(kind, entryId, tierId, groupId, text, original) {
        if (!root.manager) return
        if (kind === root.thresholdKind) {
            root.manager.editThresholdText(entryId, tierId, text)
            return
        }
        if (text === original) return
        if (kind === 2) root.manager.renameScenario(entryId, text)
        else if (groupId !== "") root.manager.renameGroup(groupId, text)
    }

    // Flushes the open editor into the session, so a following command sees the typed text even
    // though the editor never received Return or lost focus.
    function commitActiveEdit() {
        const editor = root.activeEditor
        if (!editor) return
        editor.commitNow()
        editor.ownerView.closeEditor()
    }

    // The model index a stable {entryId, tierId?, columnKind?} anchor addresses now; invalid when
    // its row is gone.
    function anchorIndex(anchor) {
        const model = root.tableModel
        const row = anchor && anchor.entryId ? model.rowForEntry(anchor.entryId) : -1
        if (row < 0) return model.index(-1, -1)
        let column = 2
        if (anchor.tierId) {
            const tierColumn = model.columnForTier(anchor.tierId)
            if (tierColumn >= 0) column = tierColumn
        } else if (anchor.columnKind !== undefined && anchor.columnKind !== "") {
            column = Number(anchor.columnKind)
        }
        return model.index(row, column)
    }

    // Where a paste lands: a rank header, the manager's current cell (group cells are forwarded so
    // the session can explain the rejection), or the end of an empty table. Null when a populated
    // table has no destination. The manager's anchor, not the visual current index, is read, so a
    // paste issued straight after a reset lands before the visual selection is rebuilt.
    function pasteDestination() {
        // An empty tierId is the placeholder header of a rankless table: the names append ranks.
        if (root.headerFocused) return {kind: "rankHeader", tierId: root.focusedTierId}
        const model = root.tableModel
        if (!model || model.rowCount() === 0) return {kind: "append"}
        const index = root.anchorIndex(root.manager.currentCell)
        if (!index.valid) return null
        const anchor = model.anchorAt(index.row, index.column)
        const info = model.rowInfo(index.row)
        switch (anchor.columnKind) {
        case 0: return {kind: "category", groupId: info.categoryId}
        case 1: return {kind: "subcategory", groupId: info.subcategoryId}
        case 2: return {kind: "scenario", entryId: anchor.entryId}
        default: return {kind: "threshold", entryId: anchor.entryId, tierId: anchor.tierId}
        }
    }

    function reportResult(result) {
        root.statusText = result && result.ok === false ? result.error : ""
        return result
    }

    function pasteText(destination, text) {
        return root.reportResult(root.manager.pasteText(destination, text))
    }

    // Commits the open editor first, so typed text is its own earlier step, then reads the
    // clipboard only now that the user asked to paste.
    function pasteFromClipboard() {
        if (!root.manager) return null
        root.commitActiveEdit()
        const destination = root.pasteDestination()
        if (!destination)
            return root.reportResult({ok: false, error: qsTr("Select a scenario cell, threshold cell or rank header before pasting.")})
        return root.pasteText(destination, root.manager.clipboardText())
    }

    // Undo restores the manager's whole selection; focusing through focusCell() would collapse it
    // to the current cell.
    function undo() {
        if (!root.manager) return null
        root.commitActiveEdit()
        const result = root.reportResult(root.manager.undo())
        if (result && result.ok === false) return result
        root.restoreSessionSelection()
        root.revealCurrent()
        return result
    }

    function revealCurrent() {
        const current = selection.currentIndex
        if (!current.valid) return
        const view = root.viewForColumn(current.column)
        view.positionViewAtCell(Qt.point(current.column, current.row), TableView.Contain)
        view.forceActiveFocus()
    }

    function handleCommandKey(event) {
        if (event.matches(StandardKey.Paste)) {
            root.pasteFromClipboard()
            event.accepted = true
        } else if (event.matches(StandardKey.Undo)) {
            root.undo()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    function viewForColumn(column) {
        return column >= root.thresholdKind ? thresholdView : identityView
    }

    function moveCurrent(row, column) {
        const model = root.tableModel
        if (!model || row < 0 || column < 0) return false
        const view = root.viewForColumn(column)
        selection.setCurrentIndex(model.index(row, column), ItemSelectionModel.ClearAndSelect)
        view.positionViewAtCell(Qt.point(column, row), TableView.Contain)
        view.forceActiveFocus()
        return true
    }

    function groupListItem(groupId) {
        for (let i = 0; i < groupRepeater.count; ++i) {
            const item = groupRepeater.itemAt(i)
            if (item && item.groupId === groupId) return item
        }
        return null
    }

    function rankHeaderItem(tierId) {
        const items = rankHeader.contentItem.children
        for (let i = 0; i < items.length; ++i)
            if (items[i].visible && items[i].tierId === tierId) return items[i]
        return null
    }

    function focusRankHeader(tierId) {
        const header = root.rankHeaderItem(tierId)
        if (header) header.forceActiveFocus()
        return header !== null
    }

    // Moves focus to a stable anchor: {entryId, tierId?, columnKind?} for a cell, {tierId} for a
    // rank header, or {groupId} for a group (its first row, or the group list when it is empty).
    function focusCell(anchor) {
        const model = root.tableModel
        if (!model || !anchor) return false
        root.commitActiveEdit()
        if (anchor.groupId) {
            const row = model.firstRowOfGroup(anchor.groupId)
            if (row < 0) {
                const item = root.groupListItem(anchor.groupId)
                if (!item) return false
                item.forceActiveFocus()
                return true
            }
            const group = (root.manager.groups || []).find(g => g.id === anchor.groupId)
            return root.moveCurrent(row, group && group.kind === "subcategory" ? 1 : 0)
        }
        if (anchor.entryId) {
            const index = root.anchorIndex(anchor)
            return index.valid && root.moveCurrent(index.row, index.column)
        }
        if (anchor.tierId) {
            const column = model.columnForTier(anchor.tierId)
            if (column < 0) return false
            thresholdView.positionViewAtColumn(column, TableView.Contain)
            rankHeader.forceLayout()
            if (!root.focusRankHeader(anchor.tierId)) Qt.callLater(root.focusRankHeader, anchor.tierId)
            return true
        }
        return false
    }

    function refreshCurrentRow() {
        const current = selection.currentIndex
        root.currentRowInfo = current.valid && root.tableModel ? root.tableModel.rowInfo(current.row) : ({})
    }

    // Row context, and the rank or group the current cell belongs to unless a rank header holds
    // the focus.
    function refreshCurrentContext() {
        root.refreshCurrentRow()
        const current = selection.currentIndex
        if (!current.valid || root.headerFocused) return
        if (current.column >= root.thresholdKind) root.focusedTierId = root.tableModel.anchorAt(current.row, current.column).tierId
        else if (current.column === 0) root.focusedGroupId = root.currentRowInfo.categoryId
        else if (current.column === 1) root.focusedGroupId = root.currentRowInfo.subcategoryId
    }

    function publishCurrent() {
        if (!root.manager || !root.tableModel || root.restoringSelection) return
        const current = selection.currentIndex
        root.manager.currentCell = current.valid ? root.tableModel.anchorAt(current.row, current.column) : ({})
        root.headerFocused = false
        root.refreshCurrentContext()
    }

    function selectedIds() {
        const ids = []
        for (const index of selection.selectedIndexes) {
            const id = root.tableModel.anchorAt(index.row, index.column).entryId
            if (id && ids.indexOf(id) === -1) ids.push(id)
        }
        return ids
    }

    function publishSelection() {
        if (!root.manager || !root.tableModel || root.restoringSelection) return
        root.manager.selectedEntryIds = root.selectedIds()
    }

    // Deferred: the selection model clears itself from the same modelReset signal, in no
    // guaranteed order, and the manager announces repaired anchors before the model resets.
    function scheduleSelectionRestore() {
        Qt.callLater(root.restoreSessionSelection)
    }

    // Rebuilds the visual selection and current index from the manager's stable anchors without
    // publishing them back or moving focus, except to a rank header that lost focus to a reset.
    function restoreSessionSelection() {
        const model = root.tableModel
        if (!root.manager || !model) {
            root.restoringSelection = false
            return
        }
        root.restoringSelection = true
        const wanted = (root.manager.selectedEntryIds || []).filter(id => model.rowForEntry(id) >= 0)
        const shown = root.selectedIds()
        if (wanted.length !== shown.length || wanted.some(id => shown.indexOf(id) === -1)) {
            selection.clearSelection()
            for (const id of wanted)
                selection.select(model.index(model.rowForEntry(id), 2), ItemSelectionModel.Select | ItemSelectionModel.Rows)
        }
        const index = root.anchorIndex(root.manager.currentCell)
        const current = selection.currentIndex
        if (index.row !== current.row || index.column !== current.column)
            selection.setCurrentIndex(index, ItemSelectionModel.NoUpdate)
        root.restoringSelection = false

        if (root.focusedTierId !== "" && model.columnForTier(root.focusedTierId) < 0) {
            root.focusedTierId = ""
            root.headerFocused = false
        }
        root.refreshCurrentContext()
        if (root.restoreHeaderFocus && root.headerFocused) root.focusCell({tierId: root.focusedTierId})
        root.restoreHeaderFocus = false
    }

    Connections {
        target: root.tableModel
        function onDataChanged() { root.refreshCurrentRow() }
        function onModelAboutToBeReset() {
            root.restoringSelection = true
            const header = root.headerFocused ? root.rankHeaderItem(root.focusedTierId) : null
            root.restoreHeaderFocus = header !== null && header.activeFocus
        }
        function onModelReset() { root.scheduleSelectionRestore() }
    }

    // The C++ manager repairs its anchors when an edit removes their row or rank; plain test
    // doubles have no signal and rely on the model reset.
    Connections {
        target: root.manager && typeof root.manager.selectionChanged === "function" ? root.manager : null
        function onSelectionChanged() { root.scheduleSelectionRestore() }
    }

    ItemSelectionModel {
        id: selection
        model: root.tableModel
        onCurrentChanged: root.publishCurrent()
        onSelectionChanged: root.publishSelection()
    }

    // Takes no focus, so clicking it never moves the table's current cell.
    component TableSwatch: AbstractButton {
        id: swatch
        required property color swatchColor
        implicitWidth: 14
        implicitHeight: 14
        focusPolicy: Qt.NoFocus
        Accessible.role: Accessible.Button
        background: Rectangle {
            color: swatch.swatchColor
            radius: 3
            border.color: swatch.hovered ? swatch.palette.highlight : swatch.palette.mid
        }
    }

    component TableCell: Rectangle {
        id: cell

        required property int row
        required property int column
        required property bool current
        required property bool selected
        required property bool editing
        required property string entryId
        required property string tierId
        required property int columnKind
        required property string displayText
        required property string editText
        required property bool hasValue
        required property string inputState
        required property var issues
        required property string categoryId
        required property string categoryName
        required property var categoryColor
        required property int categorySpanStart
        required property int categorySpanLength
        required property string subcategoryId
        required property string subcategoryName
        required property var subcategoryColor
        required property int subcategorySpanStart
        required property int subcategorySpanLength

        readonly property bool isGroup: columnKind < 2
        readonly property string groupId: columnKind === 0 ? categoryId : columnKind === 1 ? subcategoryId : ""
        readonly property int spanStart: columnKind === 0 ? categorySpanStart : subcategorySpanStart
        readonly property int spanLength: columnKind === 0 ? categorySpanLength : subcategorySpanLength
        readonly property var groupColor: columnKind === 0 ? categoryColor : subcategoryColor
        readonly property int topRow: cell.TableView.view ? cell.TableView.view.topRow : 0
        // A span shows its name once: on its first row, or on the top visible row when the first
        // row has scrolled away, so a clipped span stays readable.
        readonly property bool groupLabelShown: isGroup && spanLength > 0 && row === Math.max(spanStart, topRow)
        readonly property string groupLabelText: columnKind === 0 ? categoryName : subcategoryName
        readonly property bool lastRowOfSpan: !isGroup || spanLength === 0 || row === spanStart + spanLength - 1
        readonly property string cellText: isGroup ? (groupLabelShown ? groupLabelText : "") : displayText
        readonly property bool unnamedScenario: columnKind === 2 && displayText === ""
        readonly property string valueState: columnKind !== root.thresholdKind ? ""
            : inputState === "nonFinite" ? qsTr("Non-finite input")
            : inputState === "invalid" ? qsTr("Invalid input")
            : !hasValue ? qsTr("Missing threshold") : ""
        readonly property string stateLabel: valueState !== "" ? valueState : current ? qsTr("Selected") : ""

        objectName: "cell_" + entryId + "_" + root.columnKey(columnKind, tierId)
        implicitWidth: 80
        implicitHeight: root.rowHeight
        color: selected || current ? Qt.alpha(palette.highlight, current ? 0.35 : 0.2)
             : isGroup && groupId !== "" ? Qt.alpha(groupColor, 0.22)
             : palette.base

        Accessible.role: Accessible.Cell
        Accessible.name: cellText !== "" ? cellText : unnamedScenario ? qsTr("Unnamed scenario") : valueState
        Accessible.description: [stateLabel].concat(issues || []).filter(t => t !== "").join("; ")

        Rectangle {
            visible: cell.lastRowOfSpan
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: cell.palette.mid
        }
        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 1
            color: cell.palette.mid
        }
        Rectangle {
            // Retained input and domain issues get a textual explanation too (Accessible and
            // tooltip); the marker is only a pointer to it.
            visible: (cell.issues || []).length > 0 || cell.inputState !== ""
            anchors.top: parent.top
            anchors.left: parent.left
            width: 6
            height: 6
            color: cell.inputState !== "" ? "#E53935" : "#FFA726"
        }

        Label {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: groupSwatch.visible ? groupSwatch.width + 10 : 6
            visible: !cell.editing
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: cell.columnKind === root.thresholdKind ? Text.AlignRight : Text.AlignLeft
            elide: Text.ElideRight
            text: cell.cellText !== "" ? cell.cellText
                : cell.columnKind === root.thresholdKind ? "—"
                : cell.unnamedScenario ? qsTr("Unnamed scenario") : ""
            font.italic: cell.inputState !== "" || cell.unnamedScenario
            font.bold: cell.isGroup
            color: cell.inputState !== "" ? "#E57373"
                 : cell.unnamedScenario ? cell.palette.placeholderText : cell.palette.text
        }

        TableSwatch {
            id: groupSwatch
            objectName: "groupSwatch_" + cell.groupId
            visible: cell.groupLabelShown && cell.groupId !== "" && !cell.editing
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            swatchColor: cell.groupColor
            Accessible.name: qsTr("Colour of group %1").arg(cell.groupLabelText)
            onClicked: root.colorSwatchRequested("group", cell.groupId, cell.groupColor)
        }

        ToolTip.visible: hover.hovered && ((cell.issues || []).length > 0 || cell.valueState !== "")
        ToolTip.text: cell.Accessible.description
        HoverHandler {
            id: hover
        }

        TableView.editDelegate: TextField {
            id: editor

            property bool committed: false
            property bool cancelled: false
            // Captured once: the delegate underneath can be recycled while the editor lives.
            property int kind: -1
            property string anchorEntry
            property string anchorTier
            property string anchorGroup
            property string original
            property TableView ownerView: null

            horizontalAlignment: cell.columnKind === root.thresholdKind ? TextInput.AlignRight : TextInput.AlignLeft
            Accessible.name: qsTr("Edit %1").arg(cell.Accessible.name)

            function commitNow() {
                if (editor.committed || editor.cancelled) return
                editor.committed = true
                root.commitCell(editor.kind, editor.anchorEntry, editor.anchorTier, editor.anchorGroup,
                                editor.text, editor.original)
            }

            Component.onCompleted: {
                editor.kind = cell.columnKind
                editor.anchorEntry = cell.entryId
                editor.anchorTier = cell.tierId
                editor.anchorGroup = cell.groupId
                editor.original = cell.columnKind === root.thresholdKind ? cell.editText
                    : cell.columnKind === 2 ? cell.displayText : cell.groupLabelText
                editor.ownerView = cell.TableView.view
                editor.text = editor.original
                editor.selectAll()
                root.activeEditor = editor
            }
            // TableView closes an editor without commit() when the user taps elsewhere; typed
            // input must still reach the session, so only Escape discards it.
            Component.onDestruction: {
                if (root.activeEditor === editor) root.activeEditor = null
                if (!editor.committed && !editor.cancelled)
                    Qt.callLater(root.commitCell, editor.kind, editor.anchorEntry, editor.anchorTier,
                                 editor.anchorGroup, editor.text, editor.original)
            }
            // Paste and Undo are session commands even mid-edit: they commit the typed text as its
            // own step first, instead of the text field consuming them locally.
            Keys.onPressed: (event) => root.handleCommandKey(event)
            // TableView's editor event filter closes the editor on the Escape key press before
            // Keys sees it; the shortcut override sent ahead of that press is where the text is
            // marked discarded. While the menu is open the override is left unaccepted, so the
            // Escape shortcut below closes the menu instead and no key press reaches TableView.
            Keys.onShortcutOverride: (event) => {
                if (event.key !== Qt.Key_Escape || editorMenu.opened) return
                editor.cancelled = true
                event.accepted = true
            }
            TableView.onCommit: editor.commitNow()

            // The menu never holds focus (see below), so it cannot close itself on Escape. A window
            // shortcut outside an open popup is not matched, hence the application context; it is
            // enabled only while this menu is open.
            Shortcut {
                sequence: "Escape"
                context: Qt.ApplicationShortcut
                enabled: editorMenu.opened
                onActivated: editorMenu.close()
            }

            // Replaces the style's menu, whose Paste and Undo act on the text field alone. Session
            // commands are deferred so the triggering menu finishes before they close the editor
            // and destroy this menu with it. Neither the menu nor its items take focus: TableView
            // closes and commits the editor as soon as focus leaves it.
            ContextMenu.menu: Menu {
                id: editorMenu
                popupType: Popup.Item
                modal: false
                focus: false
                MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Cut"); enabled: editor.selectedText !== ""; onTriggered: editor.cut() }
                MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Copy"); enabled: editor.selectedText !== ""; onTriggered: editor.copy() }
                MenuItem { focusPolicy: Qt.NoFocus; objectName: "sessionPasteAction"; text: qsTr("Paste"); onTriggered: Qt.callLater(root.pasteFromClipboard) }
                MenuItem { focusPolicy: Qt.NoFocus; objectName: "sessionUndoAction"; text: qsTr("Undo"); onTriggered: Qt.callLater(root.undo) }
                MenuSeparator {}
                MenuItem { focusPolicy: Qt.NoFocus; text: qsTr("Select All"); onTriggered: editor.selectAll() }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Flow {
            id: groupList
            objectName: "groupList"
            Layout.fillWidth: true
            spacing: 4
            visible: groupRepeater.count > 0

            Repeater {
                id: groupRepeater
                model: root.manager && root.manager.groups ? root.manager.groups : []
                delegate: Button {
                    id: groupItem
                    required property var modelData
                    readonly property string groupId: groupItem.modelData.id
                    objectName: "groupListItem_" + groupItem.modelData.id
                    flat: true
                    activeFocusOnTab: true
                    text: (groupItem.modelData.kind === "subcategory" ? "↳ " : "")
                        + (groupItem.modelData.name !== "" ? groupItem.modelData.name : qsTr("(unnamed)"))
                        + (groupItem.modelData.scenarioCount === 0 ? " " + qsTr("(empty)") : "")
                        + ((groupItem.modelData.issues || []).length > 0 ? " ⚠" : "")
                    Accessible.name: text
                    Accessible.description: (groupItem.modelData.issues || []).join("; ")
                    ToolTip.visible: hovered && (groupItem.modelData.issues || []).length > 0
                    ToolTip.text: Accessible.description
                    highlighted: root.focusedGroupId === groupItem.groupId
                    onActiveFocusChanged: if (activeFocus) root.focusedGroupId = groupItem.groupId
                    onClicked: {
                        root.focusedGroupId = groupItem.groupId
                        root.focusCell({groupId: groupItem.groupId})
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.preferredWidth: root.identityWidths[0] + root.identityWidths[1] + root.identityWidths[2]
                Layout.fillHeight: true
                spacing: 0

                Row {
                    Layout.preferredHeight: root.headerHeight
                    Repeater {
                        model: [qsTr("Category"), qsTr("Subcategory"), qsTr("Scenario")]
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            width: root.identityWidths[index]
                            height: root.headerHeight
                            color: palette.button
                            border.color: palette.mid
                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                verticalAlignment: Text.AlignVCenter
                                text: parent.modelData
                                font.bold: true
                            }
                        }
                    }
                }

                TableView {
                    id: identityView
                    objectName: "identityView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.tableModel
                    selectionModel: selection
                    syncView: thresholdView
                    syncDirection: Qt.Vertical
                    columnWidthProvider: (column) => column < root.thresholdKind ? root.identityWidths[column] : 0
                    rowHeightProvider: () => root.rowHeight
                    delegate: TableCell {}

                    onActiveFocusChanged: if (activeFocus) root.headerFocused = false
                    Keys.onPressed: (event) => root.handleCommandKey(event)
                    Keys.onRightPressed: (event) => {
                        const current = selection.currentIndex
                        if (current.valid && current.column === root.thresholdKind - 1
                                && root.tableModel.columnCount() > root.thresholdKind) {
                            root.moveCurrent(current.row, root.thresholdKind)
                            event.accepted = true
                        } else {
                            event.accepted = false
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // A rankless table has no header cell to select, so this stands in as the paste
                // destination that appends ranks.
                FocusScope {
                    id: emptyRankHeader
                    objectName: "emptyRankHeader"
                    visible: thresholdView.columns <= root.thresholdKind
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.headerHeight
                    activeFocusOnTab: visible
                    Accessible.role: Accessible.ColumnHeader
                    Accessible.name: qsTr("No ranks yet; paste rank names here")
                    onActiveFocusChanged: if (activeFocus) {
                        root.focusedTierId = ""
                        root.headerFocused = true
                    }
                    onVisibleChanged: if (!visible && root.focusedTierId === "") root.headerFocused = false
                    Keys.onPressed: (event) => root.handleCommandKey(event)

                    TapHandler {
                        onTapped: emptyRankHeader.forceActiveFocus()
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: emptyRankHeader.palette.button
                        border.color: emptyRankHeader.activeFocus ? emptyRankHeader.palette.highlight : emptyRankHeader.palette.mid
                        border.width: emptyRankHeader.activeFocus ? 2 : 1
                    }
                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        font.italic: true
                        text: qsTr("No ranks: select here and paste rank names")
                    }
                }

                HorizontalHeaderView {
                    id: rankHeader
                    objectName: "rankHeader"
                    visible: !emptyRankHeader.visible
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.headerHeight
                    clip: true
                    syncView: thresholdView
                    delegate: FocusScope {
                        id: headerCell
                        required property var model
                        required property int column
                        readonly property string tierId: headerCell.model.tierId !== undefined ? headerCell.model.tierId : ""
                        readonly property string headerName: headerCell.model.display !== undefined ? headerCell.model.display : ""
                        readonly property var headerIssues: headerCell.model.issues || []
                        readonly property var headerColor: headerCell.model.headerColor !== undefined ? headerCell.model.headerColor : "transparent"
                        property bool editingName: false
                        objectName: headerCell.tierId !== "" ? "rankHeader_" + headerCell.tierId : ""
                        implicitWidth: root.thresholdWidth
                        implicitHeight: root.headerHeight
                        activeFocusOnTab: headerCell.tierId !== ""
                        Accessible.role: Accessible.ColumnHeader
                        Accessible.name: headerCell.headerName !== "" ? headerCell.headerName : qsTr("Unnamed rank")
                        Accessible.description: headerCell.headerIssues.join("; ")
                        onActiveFocusChanged: if (activeFocus) {
                            root.focusedTierId = headerCell.tierId
                            root.headerFocused = true
                        }

                        function beginRename() {
                            if (headerCell.tierId !== "") headerCell.editingName = true
                        }

                        // While the name editor is open, its keys (including Paste and Undo) stay local.
                        Keys.onPressed: (event) => {
                            if (headerCell.editingName) {
                                event.accepted = false
                            } else if (event.key === Qt.Key_F2 || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                headerCell.beginRename()
                                event.accepted = true
                            } else {
                                root.handleCommandKey(event)
                            }
                        }

                        TapHandler {
                            onTapped: headerCell.forceActiveFocus()
                            onDoubleTapped: headerCell.beginRename()
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: headerCell.palette.button
                            border.color: headerCell.activeFocus ? headerCell.palette.highlight : headerCell.palette.mid
                            border.width: headerCell.activeFocus ? 2 : 1
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 4
                            color: headerCell.headerColor
                        }
                        TableSwatch {
                            objectName: "rankSwatch_" + headerCell.tierId
                            visible: headerCell.tierId !== "" && !headerCell.editingName
                            anchors.left: parent.left
                            anchors.leftMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            swatchColor: headerCell.headerColor
                            Accessible.name: qsTr("Colour of rank %1").arg(headerCell.Accessible.name)
                            onClicked: root.colorSwatchRequested("tier", headerCell.tierId, headerCell.headerColor)
                        }
                        Loader {
                            anchors.fill: parent
                            anchors.margins: 2
                            active: headerCell.editingName
                            sourceComponent: TextField {
                                id: rankNameEditor
                                objectName: "rankHeaderEditor"
                                property bool cancelled: false
                                text: headerCell.headerName
                                Accessible.name: qsTr("Rank name")
                                Component.onCompleted: {
                                    rankNameEditor.selectAll()
                                    rankNameEditor.forceActiveFocus()
                                }
                                Keys.onEscapePressed: {
                                    rankNameEditor.cancelled = true
                                    headerCell.editingName = false
                                    headerCell.forceActiveFocus()
                                }
                                // Return keeps focus on the header; losing focus elsewhere must not steal it back.
                                onEditingFinished: {
                                    const returnFocus = rankNameEditor.activeFocus
                                    if (!rankNameEditor.cancelled && rankNameEditor.text !== headerCell.headerName)
                                        root.manager.renameTier(headerCell.tierId, rankNameEditor.text)
                                    headerCell.editingName = false
                                    if (returnFocus) headerCell.forceActiveFocus()
                                }
                            }
                        }
                        Label {
                            anchors.fill: parent
                            anchors.margins: 4
                            anchors.leftMargin: 22
                            visible: !headerCell.editingName
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            font.bold: true
                            font.italic: headerCell.headerName === ""
                            text: (headerCell.headerName !== "" ? headerCell.headerName : qsTr("Unnamed"))
                                + (headerCell.headerIssues.length > 0 ? " ⚠" : "")
                        }
                    }
                }

                TableView {
                    id: thresholdView
                    objectName: "thresholdView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.tableModel
                    selectionModel: selection
                    columnWidthProvider: (column) => column >= root.thresholdKind ? root.thresholdWidth : 0
                    rowHeightProvider: () => root.rowHeight
                    delegate: TableCell {}
                    ScrollBar.vertical: ScrollBar {}
                    ScrollBar.horizontal: ScrollBar {}

                    onActiveFocusChanged: if (activeFocus) root.headerFocused = false
                    Keys.onPressed: (event) => root.handleCommandKey(event)
                    Keys.onLeftPressed: (event) => {
                        const current = selection.currentIndex
                        if (current.valid && current.column === root.thresholdKind) {
                            root.moveCurrent(current.row, root.thresholdKind - 1)
                            event.accepted = true
                        } else {
                            event.accepted = false
                        }
                    }
                }
            }
        }
    }
}
