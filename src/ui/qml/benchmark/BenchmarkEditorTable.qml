import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

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
    // The identifying columns' comfortable widths. A table too narrow for them and one rank column
    // narrows them toward the width their content needs (never below it), each in proportion to
    // the room it has to spare; minimumTableWidth is where that runs out.
    readonly property var identityPreferredWidths: [130, 120, 200]
    readonly property var identityHeaders: [qsTr("Category"), qsTr("Subcategory"), qsTr("Scenario")]
    property int identityContentRevision: 0
    readonly property var identityContentWidths: {
        root.identityContentRevision
        const bold = text => Math.ceil(identityBoldMetrics.advanceWidth(text))
        const plain = text => Math.ceil(identityMetrics.advanceWidth(text))
        // The header labels and the add controls under the columns.
        const widths = root.identityHeaders.map(text => bold(text) + 12)
        widths[0] = Math.max(widths[0], Math.ceil(addCategoryPlus.implicitWidth))
        widths[2] = Math.max(widths[2], Math.ceil(addScenarioPlus.implicitWidth))
        // A cell's label keeps 6px on either side, or on the right clears a group's swatch (and a
        // category's add-subcategory button); see TableCell.
        const rows = root.tableModel && typeof root.tableModel.identityTexts === "function"
                     ? root.tableModel.identityTexts() : []
        for (const row of rows) {
            widths[0] = Math.max(widths[0], bold(row.category) + (row.categoryId !== "" ? 54 : 12))
            if (row.subcategoryId !== "")
                widths[1] = Math.max(widths[1], bold(row.subcategory) + 30)
            widths[2] = Math.max(widths[2], plain(row.scenario !== "" ? row.scenario : qsTr("Unnamed scenario")) + 12)
        }
        return widths.map((width, column) => Math.min(width, root.identityPreferredWidths[column]))
    }
    readonly property var identityWidths: {
        const preferred = root.identityPreferredWidths
        const content = root.identityContentWidths
        const total = preferred[0] + preferred[1] + preferred[2]
        const slack = total - (content[0] + content[1] + content[2])
        const room = tableBody.width - root.rankSlotWidth - root.rankHeaderMinimumWidth
        if (room >= total || slack <= 0) return preferred
        const give = Math.min(1, (total - room) / slack)
        return preferred.map((width, column) => Math.floor(width - (width - content[column]) * give))
    }
    onIdentityWidthsChanged: Qt.callLater(identityView.forceLayout)
    // The narrowest the table can be with every identifying column fitting its content and one
    // rank column (the widest header's width) in view; the editor never gets narrower.
    readonly property int minimumTableWidth: root.identityContentWidths[0] + root.identityContentWidths[1]
                                             + root.identityContentWidths[2] + root.rankSlotWidth
                                             + root.rankHeaderMinimumWidth
    // Every rank column has the same width (the scrollbar's position estimate relies on it): an
    // equal share of what the identifying columns and the add-rank slot leave, but never narrower
    // than the widest rank header needs, so no header is clipped; below that the ranks scroll.
    readonly property int tierCount: Math.max(0, thresholdView.columns - root.thresholdKind)
    readonly property int availableRankWidth: Math.max(0, tableBody.width - root.identityTotalWidth - root.rankSlotWidth)
    readonly property int thresholdWidth: root.tierCount > 0
                                          ? Math.max(root.rankHeaderMinimumWidth,
                                                     Math.floor(root.availableRankWidth / root.tierCount))
                                          : root.rankHeaderMinimumWidth
    // The header label's insets: past the swatch (4px margin, 14px swatch, 2px gap) on the left,
    // 2px on the right.
    readonly property int rankHeaderLabelLeftInset: 20
    readonly property int rankHeaderLabelRightInset: 2
    readonly property int rankHeaderChromeWidth: root.rankHeaderLabelLeftInset + root.rankHeaderLabelRightInset
    property int tierHeaderRevision: 0
    readonly property int rankHeaderMinimumWidth: {
        root.tierHeaderRevision
        let widest = rankHeaderMetrics.advanceWidth(root.rankHeaderText("", false))
        const headers = root.tableModel && typeof root.tableModel.tierHeaders === "function"
                        ? root.tableModel.tierHeaders() : []
        for (const header of headers)
            widest = Math.max(widest, rankHeaderMetrics.advanceWidth(root.rankHeaderText(header.name, header.hasIssues)))
        return Math.ceil(widest) + root.rankHeaderChromeWidth
    }

    // A new rank width also moves where the ranks start inside the threshold view (it skips the
    // identifying columns at that width), and a relayout only re-places the columns in view, so the
    // views are rebuilt from scratch instead, keeping the same ranks in view. Never under an open
    // cell editor: that waits until the edit ends.
    //
    // The views' left margin follows the laid-out width, not the new one, until the rebuild: the
    // scroll position is then still measured from where the ranks started at the old width.
    property int laidOutThresholdWidth: 0
    onThresholdWidthChanged: Qt.callLater(root.relayoutRankColumns)
    onActiveEditorChanged: if (!root.activeEditor) Qt.callLater(root.relayoutRankColumns)
    function relayoutRankColumns() {
        const previous = root.laidOutThresholdWidth
        const width = root.thresholdWidth
        if (root.activeEditor) return
        if (previous === width) {
            root.restoreRankScrollAfterReset()
            root.clampRankScroll()
            return
        }
        root.laidOutThresholdWidth = width
        if (previous === 0 || !root.tableModel) {
            thresholdView.forceLayout()
            rankHeader.forceLayout()
            root.clampRankScroll()
            return
        }
        const scrolledRanks = root.rankScrollOnReset >= 0
                              ? root.rankScrollOnReset
                              : Math.max(0, thresholdView.contentX - root.thresholdKind * previous) / previous
        const contentY = thresholdView.contentY
        const hadFocus = thresholdView.activeFocus
        // The rebuild may clear the selection model; the manager's anchors put it back.
        root.restoringSelection = true
        thresholdView.model = null
        thresholdView.model = Qt.binding(() => root.tableModel)
        thresholdView.forceLayout()
        thresholdView.contentY = contentY
        thresholdView.contentX = root.rankScrollStart + Math.min(scrolledRanks * width, root.rankScrollRange)
        if (thresholdView.columns === root.tableModel.columnCount()) root.rankScrollOnReset = -1
        rankHeader.forceLayout()
        if (hadFocus) thresholdView.forceActiveFocus()
        root.scheduleSelectionRestore()
    }

    // A model reset (another benchmark opened, a rank or row added) is rebuilt by the threshold view
    // on its own schedule, which can come after the relayout above, and that rebuild keeps the
    // scroll position in columns of the width laid out before it: a wider rank then lands part-way
    // into a column. So the ranks scrolled past the first are noted when the reset starts and put
    // back once the view has caught up with the new model at the current width. -1: none pending.
    property real rankScrollOnReset: -1
    function noteRankScrollBeforeReset() {
        const width = root.laidOutThresholdWidth
        if (width <= 0 || root.rankScrollOnReset >= 0) return
        root.rankScrollOnReset = Math.max(0, thresholdView.contentX - root.rankScrollStart) / width
    }
    function restoreRankScrollAfterReset() {
        if (root.rankScrollOnReset < 0 || root.activeEditor || !root.tableModel) return
        if (thresholdView.columns !== root.tableModel.columnCount()
                || root.laidOutThresholdWidth !== root.thresholdWidth) return
        const target = root.rankScrollStart + Math.min(root.rankScrollOnReset * root.thresholdWidth,
                                                       root.rankScrollRange)
        root.rankScrollOnReset = -1
        if (Math.abs(target - thresholdView.contentX) > 0.5) thresholdView.contentX = target
    }

    // The threshold view's horizontal scroll range: from the first rank right after the scenario
    // column to the last rank ending at the add-rank slot. Ranks that fit leave no range at all.
    readonly property real rankScrollStart: root.hiddenThresholdWidth
    readonly property real rankScrollRange: Math.max(0, root.rankColumnsWidth - root.availableRankWidth)
    // A width or rank-count change can leave the view outside that range (scrolled while the ranks
    // overflowed, then they fit; or a blank stretch past the last rank). Pull it back to the nearest
    // edge; a position inside the range, such as one the user scrolled to, is kept.
    onRankScrollRangeChanged: Qt.callLater(root.relayoutRankColumns)
    function clampRankScroll() {
        if (root.activeEditor || root.laidOutThresholdWidth !== root.thresholdWidth) return
        const start = root.rankScrollStart
        const clamped = Math.min(Math.max(thresholdView.contentX, start), start + root.rankScrollRange)
        if (Math.abs(clamped - thresholdView.contentX) > 0.5) thresholdView.contentX = clamped
    }

    function rankHeaderText(name, hasIssues) {
        return (name !== "" ? name : qsTr("Unnamed")) + (hasIssues ? " ⚠" : "")
    }

    FontMetrics {
        id: rankHeaderMetrics
        font.bold: true
    }
    FontMetrics {
        id: identityBoldMetrics
        font.bold: true
    }
    FontMetrics {
        id: identityMetrics
    }
    Connections {
        target: root.tableModel
        function onModelAboutToBeReset() { root.noteRankScrollBeforeReset() }
        function onModelReset() {
            root.tierHeaderRevision++
            root.identityContentRevision++
        }
        function onDataChanged() { root.identityContentRevision++ }
        function onHeaderDataChanged() { root.tierHeaderRevision++ }
        function onColumnsInserted() { root.tierHeaderRevision++ }
        function onColumnsRemoved() { root.tierHeaderRevision++ }
    }
    // Width of the identifying columns inside the threshold view's own column space. They are as
    // wide as the ranks: every column the same width keeps the scrollbar's estimate exact.
    // Until the first layout it follows the live width; afterwards, the laid-out one (see
    // relayoutRankColumns).
    readonly property int hiddenThresholdWidth: root.thresholdKind * (root.laidOutThresholdWidth > 0
                                                                      ? root.laidOutThresholdWidth
                                                                      : root.thresholdWidth)
    function thresholdColumnWidth(column) {
        return root.thresholdWidth
    }
    readonly property int identityTotalWidth: root.identityWidths[0] + root.identityWidths[1] + root.identityWidths[2]
    // The slot after the last rank header: the "add rank" button, or its name field while one is
    // being typed. The threshold body keeps a gutter of the same width, so header and body end
    // together and a rank is never clipped under the button.
    readonly property int rankSlotWidth: root.addingRank ? 130 : 30
    // Height of the always-visible horizontal scrollbar, which sits under the threshold body when
    // the ranks do not all fit.
    readonly property int horizontalBarReserve: thresholdHorizontalBar.visible ? thresholdHorizontalBar.height : 0
    readonly property int tableContentHeight: root.headerHeight + thresholdView.rows * root.rowHeight
                                              + root.horizontalBarReserve
    // The width the rank columns take when all of them are laid out.
    readonly property int rankColumnsWidth: Math.max(0, thresholdView.columns - root.thresholdKind) * root.thresholdWidth

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
    property var selectedCellAnchors: []
    property var dragEntryIds: []
    property bool draggingRows: false
    property string hoverDropGroupId: ""
    // {row, above}: the gap a row drop would land in, while hovering between rows.
    property var dropInsertion: null
    property bool addingRank: false
    property bool addingCategory: false
    // (request) => bool, answered synchronously; `request` is {kind: "category", name},
    // {kind: "subcategory", categoryId, name} or {kind: "rank", name}.
    property var creationHandler: null
    // The entry whose scenario cell opens for editing once its row has a delegate.
    property string pendingScenarioEdit: ""

    // `kind` is "tier" or "group"; the dialog owns the colour picker.
    signal colorSwatchRequested(string kind, string id, color current)
    signal addScenarioRequested()
    // An empty `beforeEntryId` appends to the group.
    signal moveSelectionRequested(var entryIds, string targetGroupId, string beforeEntryId)
    signal rowContextRequested(string entryId, real x, real y)
    signal groupContextRequested(string groupId, real x, real y)
    signal rankContextRequested(string tierId, real x, real y)
    signal scenarioIdentityRequested(string entryId, string name, string hash)
    signal renameGroupRequested(string groupId, string name)

    readonly property var kindKeys: ["category", "subcategory", "scenario"]
    readonly property int thresholdKind: 3

    // A row with no subcategory sits straight under its category (or in Uncategorized): the data
    // layer never mixes direct scenarios and subcategories in one category. Its Category cell then
    // spans the Subcategory column, which has nothing to show for that row.
    function spansSubcategory(columnKind, subcategoryId) {
        return columnKind === 0 && subcategoryId === ""
    }
    function coveredBySpan(columnKind, subcategoryId) {
        return columnKind === 1 && subcategoryId === ""
    }
    // The current cell never rests on a covered Subcategory cell: keyboard moves (or a click on
    // the span, which already lands on the Category cell) end on the spanning Category cell.
    function redirectCoveredCurrent() {
        const current = selection.currentIndex
        if (root.restoringSelection || !current.valid || !root.tableModel) return false
        if (!root.coveredBySpan(current.column, String(root.tableModel.rowInfo(current.row).subcategoryId || "")))
            return false
        root.moveCurrent(current.row, 0)
        return true
    }

    function columnKey(kind, tierId) {
        return kind === root.thresholdKind ? tierId : root.kindKeys[kind]
    }

    function requestCreation(request) {
        return root.creationHandler ? !!root.creationHandler(request) : false
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

    function selectCell(row, column, modifiers) {
        const model = root.tableModel
        if (!model) return
        const index = model.index(row, column)
        const current = selection.currentIndex
        if (modifiers & Qt.ShiftModifier && current.valid) {
            if (!(modifiers & Qt.ControlModifier)) selection.clearSelection()
            for (let r = Math.min(current.row, row); r <= Math.max(current.row, row); ++r)
                for (let c = Math.min(current.column, column); c <= Math.max(current.column, column); ++c)
                    selection.select(model.index(r, c), ItemSelectionModel.Select)
            selection.setCurrentIndex(index, ItemSelectionModel.NoUpdate)
        } else if (modifiers & Qt.ControlModifier) {
            selection.select(index, ItemSelectionModel.Toggle)
            selection.setCurrentIndex(index, ItemSelectionModel.NoUpdate)
        } else {
            selection.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect)
        }
        root.viewForColumn(column).forceActiveFocus()
    }

    function groupListItem(groupId) {
        for (let i = 0; i < groupRepeater.count; ++i) {
            const item = groupRepeater.itemAt(i)
            if (item && item.groupId === groupId) return item.focusButton
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

    function beginScenarioEdit(entryId) {
        if (!root.focusCell({entryId: entryId, columnKind: 2})) return false
        if (root.tableModel.rowForEntry(entryId) < 0) return false
        root.pendingScenarioEdit = entryId
        pendingEditExpiry.restart()
        root.runPendingScenarioEdit()
        return true
    }

    // The table is capped at its content, so a row that was just added lies outside the viewport,
    // with no delegate yet, until the layout has grown to take it. The edit opens as soon as the
    // cell is loaded; layout changes of the view retry it.
    function runPendingScenarioEdit() {
        const entryId = root.pendingScenarioEdit
        if (entryId === "") return
        const row = root.tableModel ? root.tableModel.rowForEntry(entryId) : -1
        if (row < 0) {
            root.pendingScenarioEdit = ""
            return
        }
        root.moveCurrent(row, 2)
        const index = root.tableModel.index(row, 2)
        if (identityView.itemAtIndex(index) === null) return
        root.pendingScenarioEdit = ""
        pendingEditExpiry.stop()
        identityView.edit(index)
    }

    function beginGroupRename(groupId) {
        const row = root.tableModel.firstRowOfGroup(groupId)
        if (row < 0) {
            const empty = root.groupListItem(groupId)
            if (!empty) return false
            empty.parent.renaming = true
            empty.parent.renameField.text = empty.parent.modelData.name
            empty.parent.renameField.forceActiveFocus()
            return true
        }
        const group = (root.manager.groups || []).find(item => item.id === groupId)
        const column = group && group.kind === "subcategory" ? 1 : 0
        root.moveCurrent(row, column)
        identityView.edit(root.tableModel.index(row, column))
        return true
    }

    function beginSubcategoryCreation(groupId) {
        const empty = root.groupListItem(groupId)
        if (empty) {
            empty.parent.addingSubcategory = true
            empty.parent.subcategoryField.forceActiveFocus()
            return true
        }
        for (const item of identityView.contentItem.children) {
            if (item.groupId === groupId && item.columnKind === 0 && item.groupLabelShown) {
                item.addingSubcategory = true
                item.subcategoryField.forceActiveFocus()
                return true
            }
        }
        return false
    }

    function beginRankRename(tierId) {
        const header = root.rankHeaderItem(tierId)
        if (!header) return false
        header.beginRename()
        return true
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

    function selectionHasEntry(entryId) {
        return root.selectedIds().indexOf(entryId) !== -1
    }

    function canDropOn(groupId) {
        if (groupId === "") return true
        const groups = root.manager && root.manager.groups ? root.manager.groups : []
        const group = groups.find(item => item.id === groupId)
        return group !== undefined && group.acceptsScenarios === true
    }

    function beginRowDrag(entryId) {
        const selected = root.manager && root.manager.selectedEntryIds ? root.manager.selectedEntryIds : []
        root.dragEntryIds = selected.indexOf(entryId) !== -1 ? selected.slice() : [entryId]
        root.draggingRows = true
    }

    function dropRows(groupId, beforeEntryId) {
        if (!root.draggingRows || !root.canDropOn(groupId) || root.dragEntryIds.length === 0) return
        root.moveSelectionRequested(root.dragEntryIds.slice(), groupId, beforeEntryId || "")
        root.draggingRows = false
        root.dragEntryIds = []
    }

    // The item-local point under `scenePoint`, or null when it falls outside the visible item.
    function localPointIn(item, scenePoint) {
        if (!item || !item.visible) return null
        const local = item.mapFromItem(null, scenePoint.x, scenePoint.y)
        return local.x >= 0 && local.y >= 0 && local.x < item.width && local.y < item.height ? local : null
    }

    // Whole-group targets, which append: a group list button, a group cell or the Uncategorized zone.
    function targetAtScene(scenePoint) {
        for (let i = 0; i < groupRepeater.count; ++i) {
            const item = groupRepeater.itemAt(i)
            if (item && root.localPointIn(item.focusButton, scenePoint)) return item.groupId
        }
        for (const item of identityView.contentItem.children) {
            if (item.isGroup && item.groupId !== "" && root.localPointIn(item.face, scenePoint)) return item.groupId
        }
        if (root.localPointIn(uncategorizedDropZone, scenePoint)) return ""
        return null
    }

    // The gap above or below the scenario or threshold cell under the point, as the group the row
    // lives in and the entry the drop lands before ("" for the end of that group).
    function rowInsertionAtScene(scenePoint) {
        const model = root.tableModel
        if (!model) return null
        const containerOf = info => info.subcategoryId !== "" ? info.subcategoryId : info.categoryId
        for (const view of [identityView, thresholdView]) {
            for (const item of view.contentItem.children) {
                if (item.columnKind === undefined || item.columnKind < 2) continue
                const local = root.localPointIn(item, scenePoint)
                if (!local) continue
                const groupId = containerOf(model.rowInfo(item.row))
                const above = local.y < item.height / 2
                let beforeEntryId = item.entryId
                if (!above) {
                    const next = model.rowInfo(item.row + 1)
                    beforeEntryId = next.entryId !== undefined && containerOf(next) === groupId ? next.entryId : ""
                }
                return {groupId: groupId, beforeEntryId: beforeEntryId, row: item.row, above: above}
            }
        }
        return null
    }

    function hoverAtScene(scenePoint) {
        const target = root.targetAtScene(scenePoint)
        root.hoverDropGroupId = target !== null && root.canDropOn(target)
            ? target === "" ? "__uncategorized__" : target : ""
        const insertion = target === null ? root.rowInsertionAtScene(scenePoint) : null
        root.dropInsertion = insertion ? {row: insertion.row, above: insertion.above} : null
    }

    function dropAtScene(scenePoint) {
        const target = root.targetAtScene(scenePoint)
        if (target !== null) {
            root.dropRows(target)
        } else {
            const insertion = root.rowInsertionAtScene(scenePoint)
            if (insertion) root.dropRows(insertion.groupId, insertion.beforeEntryId)
        }
        root.hoverDropGroupId = ""
        root.dropInsertion = null
    }

    function publishSelection() {
        if (!root.manager || !root.tableModel || root.restoringSelection) return
        root.selectedCellAnchors = selection.selectedIndexes.map(index => root.tableModel.anchorAt(index.row, index.column))
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
            const anchors = root.selectedCellAnchors
            const anchorIds = [...new Set(anchors.map(anchor => anchor.entryId))]
            if (anchors.length > 0 && anchorIds.length === wanted.length
                    && wanted.every(id => anchorIds.indexOf(id) !== -1)) {
                for (const anchor of anchors) {
                    const restored = root.anchorIndex(anchor)
                    if (restored.valid) selection.select(restored, ItemSelectionModel.Select)
                }
            } else {
                for (const id of wanted)
                    selection.select(model.index(model.rowForEntry(id), 2), ItemSelectionModel.Select | ItemSelectionModel.Rows)
            }
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

    // Gives up on a pending edit whose cell never appears (for example, the row was removed).
    Timer {
        id: pendingEditExpiry
        interval: 1000
        onTriggered: root.pendingScenarioEdit = ""
    }

    Connections {
        target: identityView
        function onBottomRowChanged() { root.runPendingScenarioEdit() }
        function onRowsChanged() { root.runPendingScenarioEdit() }
        function onHeightChanged() { root.runPendingScenarioEdit() }
    }

    ItemSelectionModel {
        id: selection
        model: root.tableModel
        onCurrentChanged: if (!root.redirectCoveredCurrent()) root.publishCurrent()
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

    // A labelled "+ Add ..." control that extends the table. It keeps keyboard focus (Tab) and
    // shows what it does on hover.
    component AddRowButton: ToolButton {
        id: addRowButton
        // A HoverHandler rather than `hovered`: the latter stays false where the platform turns
        // hover effects off.
        HoverHandler { id: addRowHover }
        ToolTip.visible: addRowHover.hovered
        ToolTip.delay: Theme.toolTipDelay
        background: Rectangle {
            color: addRowHover.hovered || addRowButton.activeFocus
                ? Qt.alpha(addRowButton.palette.highlight, 0.25) : Qt.alpha(addRowButton.palette.button, 0.35)
            border.color: addRowButton.activeFocus ? addRowButton.palette.highlight : addRowButton.palette.mid
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
        readonly property bool rowHighlighted: root.selectionHasEntry(entryId)
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
        property bool addingSubcategory: false
        readonly property alias subcategoryField: subcategoryCreationField

        // A category cell whose rows have no subcategory (scenarios straight under the category,
        // or Uncategorized) spans the Subcategory column too; that row's Subcategory cell is
        // covered: hidden and inert, while the spanning cell draws over it.
        readonly property bool spansBoth: root.spansSubcategory(columnKind, subcategoryId)
        readonly property bool coveredBySpan: root.coveredBySpan(columnKind, subcategoryId)
        // What the cell draws and where it takes pointer input: wider than the cell when it spans.
        readonly property alias face: face

        objectName: "cell_" + entryId + "_" + root.columnKey(columnKind, tierId)
        implicitWidth: 80
        implicitHeight: root.rowHeight
        // TableView sizes every cell to its column, so a spanning cell draws past its right edge,
        // above the covered neighbour.
        z: spansBoth ? 1 : 0
        color: "transparent"
        opacity: coveredBySpan ? 0 : 1
        enabled: !coveredBySpan

        Accessible.role: Accessible.Cell
        Accessible.ignored: coveredBySpan
        Accessible.name: cellText !== "" ? cellText : unnamedScenario ? qsTr("Unnamed scenario") : valueState
        Accessible.description: [stateLabel].concat(issues || []).filter(t => t !== "").join("; ")

        Rectangle {
            id: face
            objectName: "cellFace"
            width: cell.spansBoth ? root.identityWidths[0] + root.identityWidths[1] : cell.width
            height: cell.height
            color: cell.current ? Qt.alpha(palette.highlight, 0.48)
                 : cell.selected ? Qt.alpha(palette.highlight, 0.3)
                 : cell.rowHighlighted ? Qt.alpha(palette.highlight, 0.14)
                 : cell.isGroup && cell.groupId !== "" && root.hoverDropGroupId === cell.groupId ? Qt.alpha(palette.highlight, 0.45)
                 : cell.isGroup && cell.groupId !== "" ? Qt.alpha(cell.groupColor, Theme.groupTintAlpha)
                 : palette.base

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
                color: cell.inputState !== "" ? Theme.error : Theme.warning
            }
            Rectangle {
                readonly property var insertion: root.dropInsertion
                visible: cell.columnKind >= 2 && insertion !== null && insertion.row === cell.row
                y: insertion && insertion.above ? 0 : parent.height - height
                width: parent.width
                height: 2
                color: cell.palette.highlight
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: groupSwatch.visible ? groupSwatch.width + (addSubcategoryPlus.visible ? 34 : 10) : 6
                visible: !cell.editing && !cell.addingSubcategory
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: cell.columnKind === root.thresholdKind ? Text.AlignRight : Text.AlignLeft
                elide: Text.ElideRight
                text: cell.cellText !== "" ? cell.cellText
                    : cell.columnKind === root.thresholdKind ? "—"
                    : cell.unnamedScenario ? qsTr("Unnamed scenario") : ""
                font.italic: cell.inputState !== "" || cell.unnamedScenario
                font.bold: cell.isGroup
                color: cell.inputState !== "" ? Theme.errorText
                     : cell.unnamedScenario ? cell.palette.placeholderText : cell.palette.text
            }

            TableSwatch {
                id: groupSwatch
                objectName: "groupSwatch_" + cell.groupId
                visible: cell.groupLabelShown && cell.groupId !== "" && !cell.editing && !cell.addingSubcategory
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                swatchColor: cell.groupColor
                Accessible.name: qsTr("Colour of group %1").arg(cell.groupLabelText)
                onClicked: root.colorSwatchRequested("group", cell.groupId, cell.groupColor)
            }

            ToolButton {
                id: addSubcategoryPlus
                objectName: "addSubcategoryPlus_" + cell.categoryId
                visible: cell.columnKind === 0 && cell.groupLabelShown && cell.categoryId !== ""
                         && !cell.editing && !cell.addingSubcategory
                anchors.right: groupSwatch.left
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                text: "+"
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Add subcategory to %1").arg(cell.categoryName)
                HoverHandler { id: addSubcategoryHover }
                ToolTip.visible: addSubcategoryHover.hovered
                ToolTip.delay: Theme.toolTipDelay
                ToolTip.text: Accessible.name
                onClicked: {
                    cell.addingSubcategory = true
                    subcategoryCreationField.forceActiveFocus()
                }
            }

            TextField {
                id: subcategoryCreationField
                objectName: "subcategoryCreationField"
                visible: cell.addingSubcategory
                anchors.fill: parent
                placeholderText: qsTr("Subcategory name")
                Accessible.name: placeholderText
                // Focus goes back to the table before the request is sent: the request may open
                // the Manager's modal "already holds scenarios" prompt, which hands focus back on
                // closing to whatever held it when it opened, and this field is hidden by then. A
                // hidden field must not stay the table's focus child either, or focusing the table
                // (a focus scope) would hand active focus straight back to it.
                function releaseFocus() {
                    subcategoryCreationField.focus = false
                    cell.TableView.view.forceActiveFocus()
                }
                onAccepted: {
                    if (text.trim() === "") return
                    subcategoryCreationField.releaseFocus()
                    if (root.requestCreation({kind: "subcategory", categoryId: cell.categoryId, name: text})) {
                        cell.addingSubcategory = false
                        text = ""
                    } else {
                        subcategoryCreationField.forceActiveFocus()
                    }
                }
                Keys.onEscapePressed: {
                    cell.addingSubcategory = false
                    text = ""
                    subcategoryCreationField.releaseFocus()
                }
                // TextInput emits accepted() but leaves Return unaccepted, so it would propagate
                // to the TableView, whose edit trigger then opens a rename editor on the current
                // (category) cell, behind the prompt the name may have opened.
                Keys.priority: Keys.AfterItem
                Keys.onReturnPressed: (event) => event.accepted = true
                Keys.onEnterPressed: (event) => event.accepted = true
            }

            DragHandler {
                id: rowDrag
                target: null
                enabled: cell.columnKind >= 2 && !cell.editing
                onActiveChanged: {
                    if (active) root.beginRowDrag(cell.entryId)
                    else if (root.draggingRows) {
                        root.dropAtScene(rowDrag.centroid.scenePosition)
                        root.draggingRows = false
                        root.dragEntryIds = []
                        root.dropInsertion = null
                    }
                }
                onTranslationChanged: if (active) root.hoverAtScene(rowDrag.centroid.scenePosition)
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => {
                    const point = cell.mapToItem(root, eventPoint.position.x, eventPoint.position.y)
                    if (cell.isGroup && cell.groupId !== "") {
                        root.focusedGroupId = cell.groupId
                        root.groupContextRequested(cell.groupId, point.x, point.y)
                    } else if (!cell.isGroup) {
                        if (!root.selectionHasEntry(cell.entryId))
                            root.moveCurrent(cell.row, cell.column)
                        else
                            selection.setCurrentIndex(root.tableModel.index(cell.row, cell.column), ItemSelectionModel.NoUpdate)
                        root.rowContextRequested(cell.entryId, point.x, point.y)
                    }
                }
            }

            TapHandler {
                id: selectTap
                acceptedButtons: Qt.LeftButton
                enabled: !cell.editing && !cell.addingSubcategory
                onTapped: root.selectCell(cell.row, cell.column, selectTap.point.modifiers)
                onDoubleTapped: {
                    root.selectCell(cell.row, cell.column, selectTap.point.modifiers)
                    if (cell.columnKind >= 2 || cell.groupId !== "")
                        cell.TableView.view.edit(root.tableModel.index(cell.row, cell.column))
                }
            }
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
            property int suggestionIndex: 0
            readonly property var suggestionPopupView: suggestionPopup

            // A spanning category's name is edited across the whole span.
            width: cell.face.width
            horizontalAlignment: cell.columnKind === root.thresholdKind ? TextInput.AlignRight : TextInput.AlignLeft
            Accessible.name: qsTr("Edit %1").arg(cell.Accessible.name)

            function commitNow() {
                if (editor.committed || editor.cancelled) return
                editor.committed = true
                root.commitCell(editor.kind, editor.anchorEntry, editor.anchorTier, editor.anchorGroup,
                                editor.text, editor.original)
            }

            function suggestions() {
                const query = editor.text.trim().toLowerCase()
                if (editor.kind !== 2 || query === "" || !root.manager) return []
                const catalogue = root.manager.scenarioCatalogue || []
                return catalogue.filter(item => item.name.toLowerCase().indexOf(query) !== -1)
            }

            function chooseSuggestion(item) {
                if (!item) return
                const entryId = editor.anchorEntry
                editor.cancelled = true
                suggestionPopup.close()
                root.activeEditor = null
                Qt.callLater(root.scenarioIdentityRequested, entryId, item.name, item.hash)
                editor.ownerView.closeEditor()
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
            Keys.onPressed: (event) => {
                if (suggestionPopup.opened && event.key === Qt.Key_Down) {
                    editor.suggestionIndex = Math.min(editor.suggestionIndex + 1, editor.suggestions().length - 1)
                    event.accepted = true
                } else if (suggestionPopup.opened && event.key === Qt.Key_Up) {
                    editor.suggestionIndex = Math.max(editor.suggestionIndex - 1, 0)
                    event.accepted = true
                } else if (suggestionPopup.opened && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                    editor.chooseSuggestion(editor.suggestions()[editor.suggestionIndex])
                    event.accepted = true
                } else {
                    root.handleCommandKey(event)
                }
            }
            onTextEdited: {
                editor.suggestionIndex = 0
                if (editor.suggestions().length > 0) suggestionPopup.open()
                else suggestionPopup.close()
            }
            // TableView's editor event filter closes the editor on the Escape key press before
            // Keys sees it; the shortcut override sent ahead of that press is where the text is
            // marked discarded. While the menu or the suggestion list is open the override is left
            // unaccepted, so the Escape shortcut below closes it instead and no key press reaches
            // TableView.
            Keys.onShortcutOverride: (event) => {
                if (suggestionPopup.opened && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                    event.accepted = true
                    editor.chooseSuggestion(editor.suggestions()[editor.suggestionIndex])
                    return
                }
                if (event.key !== Qt.Key_Escape || editorMenu.opened || suggestionPopup.opened) return
                editor.cancelled = true
                event.accepted = true
            }
            TableView.onCommit: editor.commitNow()

            // Neither the menu nor the suggestion list holds focus (see below), so they cannot close
            // themselves on Escape. A window shortcut outside an open popup is not matched, hence
            // the application context; it is enabled only while one of them is open. The menu is
            // the more recent popup, so it closes first.
            Shortcut {
                objectName: "editorEscapeShortcut"
                sequence: "Escape"
                context: Qt.ApplicationShortcut
                enabled: editorMenu.opened || suggestionPopup.opened
                onActivated: {
                    if (editorMenu.opened) editorMenu.close()
                    else suggestionPopup.close()
                }
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

            Popup {
                id: suggestionPopup
                parent: editor
                x: 0
                y: editor.height
                width: Math.max(editor.width, 240)
                height: Math.min(suggestionList.contentHeight + 8, 180)
                popupType: Popup.Item
                modal: false
                focus: false
                closePolicy: Popup.NoAutoClose
                padding: 4
                contentItem: ListView {
                    id: suggestionList
                    clip: true
                    model: editor.suggestions()
                    delegate: ItemDelegate {
                        id: suggestion
                        required property var modelData
                        required property int index
                        objectName: "scenarioSuggestion_" + suggestion.modelData.hash
                        width: suggestionList.width
                        text: qsTr("%1 · %2").arg(suggestion.modelData.name).arg(suggestion.modelData.hash)
                        highlighted: index === editor.suggestionIndex
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTr("Use played scenario %1, hash %2")
                            .arg(suggestion.modelData.name).arg(suggestion.modelData.hash)
                        onClicked: editor.chooseSuggestion(suggestion.modelData)
                    }
                }
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
                model: root.manager && root.manager.groups
                    ? root.manager.groups.filter(group => group.scenarioCount === 0 && group.kind !== "uncategorized") : []
                delegate: Row {
                    id: groupItem
                    required property var modelData
                    readonly property string groupId: groupItem.modelData.id
                    readonly property alias focusButton: groupButton
                    property bool addingSubcategory: false
                    property bool renaming: false
                    readonly property alias subcategoryField: emptySubcategoryName
                    readonly property alias renameField: emptyGroupRenameField
                    Button {
                        id: groupButton
                        objectName: "groupListItem_" + groupItem.groupId
                        flat: true
                        visible: !groupItem.renaming
                        activeFocusOnTab: true
                        text: (groupItem.modelData.kind === "subcategory" ? "↳ " : "")
                            + (groupItem.modelData.name !== "" ? groupItem.modelData.name : qsTr("(unnamed)"))
                            + " " + qsTr("(empty)")
                            + ((groupItem.modelData.issues || []).length > 0 ? " ⚠" : "")
                        Accessible.name: text
                        Accessible.description: (groupItem.modelData.issues || []).join("; ")
                        ToolTip.visible: hovered && (groupItem.modelData.issues || []).length > 0
                        ToolTip.text: Accessible.description
                        highlighted: root.focusedGroupId === groupItem.groupId
                                     || root.hoverDropGroupId === groupItem.groupId
                        onActiveFocusChanged: if (activeFocus) root.focusedGroupId = groupItem.groupId
                        onClicked: root.focusedGroupId = groupItem.groupId
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                const p = groupButton.mapToItem(root, 0, groupButton.height)
                                root.focusedGroupId = groupItem.groupId
                                root.groupContextRequested(groupItem.groupId, p.x, p.y)
                            }
                        }
                    }
                    ToolButton {
                        visible: groupItem.modelData.kind === "category" && !groupItem.addingSubcategory && !groupItem.renaming
                        text: "+"
                        Accessible.name: qsTr("Add subcategory to %1").arg(groupItem.modelData.name)
                        HoverHandler { id: emptyGroupAddHover }
                        ToolTip.visible: emptyGroupAddHover.hovered
                        ToolTip.delay: Theme.toolTipDelay
                        ToolTip.text: Accessible.name
                        onClicked: {
                            groupItem.addingSubcategory = true
                            emptySubcategoryName.forceActiveFocus()
                        }
                    }
                    TextField {
                        id: emptySubcategoryName
                        visible: groupItem.addingSubcategory
                        width: 130
                        placeholderText: qsTr("Subcategory name")
                        onAccepted: {
                            if (text.trim() === "") return
                            if (root.requestCreation({kind: "subcategory", categoryId: groupItem.groupId, name: text})) {
                                groupItem.addingSubcategory = false
                                text = ""
                            }
                        }
                        Keys.onEscapePressed: {
                            groupItem.addingSubcategory = false
                            text = ""
                        }
                    }
                    TextField {
                        id: emptyGroupRenameField
                        objectName: "emptyGroupRenameField_" + groupItem.groupId
                        visible: groupItem.renaming
                        width: 140
                        Accessible.name: qsTr("Group name")
                        onAccepted: {
                            root.renameGroupRequested(groupItem.groupId, text)
                            groupItem.renaming = false
                        }
                        Keys.onEscapePressed: groupItem.renaming = false
                    }
                }
            }
        }

        // The table is capped at its own content, so the add-category and add-scenario controls
        // below sit directly under the last row; taller content scrolls inside the cap.
        RowLayout {
            id: tableBody
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: root.tableContentHeight
            Layout.maximumHeight: root.tableContentHeight
            spacing: 0

            ColumnLayout {
                Layout.preferredWidth: root.identityTotalWidth
                Layout.minimumWidth: root.identityTotalWidth
                Layout.maximumWidth: root.identityTotalWidth
                Layout.fillHeight: true
                spacing: 0

                Row {
                    Layout.preferredHeight: root.headerHeight
                    Repeater {
                        model: root.identityHeaders
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
                    selectionBehavior: TableView.SelectCells
                    selectionMode: TableView.ExtendedSelection
                    pointerNavigationEnabled: false
                    syncView: thresholdView
                    syncDirection: Qt.Vertical
                    columnWidthProvider: (column) => column < root.thresholdKind ? root.identityWidths[column] : 0
                    rowHeightProvider: () => root.rowHeight
                    delegate: TableCell {}

                    onActiveFocusChanged: if (activeFocus) root.headerFocused = false
                    Keys.onPressed: (event) => root.handleCommandKey(event)
                    Keys.onLeftPressed: (event) => {
                        const current = selection.currentIndex
                        if (current.valid && current.column === 2
                                && root.spansSubcategory(0, String(root.tableModel.rowInfo(current.row).subcategoryId || ""))) {
                            root.moveCurrent(current.row, 0)
                            event.accepted = true
                        } else {
                            event.accepted = false
                        }
                    }
                    Keys.onRightPressed: (event) => {
                        const current = selection.currentIndex
                        if (current.valid && current.column === 0
                                && root.spansSubcategory(0, String(root.tableModel.rowInfo(current.row).subcategoryId || ""))) {
                            root.moveCurrent(current.row, 2)
                            event.accepted = true
                        } else if (current.valid && current.column === root.thresholdKind - 1
                                && root.tableModel.columnCount() > root.thresholdKind) {
                            root.moveCurrent(current.row, root.thresholdKind)
                            event.accepted = true
                        } else {
                            event.accepted = false
                        }
                    }
                }

                // Mirrors the threshold side's scrollbar, so both views are the same height.
                Item {
                    Layout.preferredHeight: root.horizontalBarReserve
                    visible: root.horizontalBarReserve > 0
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // A rankless table has no header cell to select, so this stands in as the paste
                // destination that appends ranks.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.headerHeight
                    spacing: 0
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
                    Layout.maximumWidth: root.rankColumnsWidth
                    Layout.preferredHeight: root.headerHeight
                    clip: true
                    syncView: thresholdView
                    // The same widths as the body: columns the body has not loaded (the identifying
                    // ones, once scrolled away) must not fall back to the delegate's implicit width,
                    // or the header drifts three columns away from its body.
                    leftMargin: -root.hiddenThresholdWidth
                    columnWidthProvider: (column) => root.thresholdColumnWidth(column)
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
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: eventPoint => {
                                headerCell.forceActiveFocus()
                                const point = headerCell.mapToItem(root, eventPoint.position.x, eventPoint.position.y)
                                root.rankContextRequested(headerCell.tierId, point.x, point.y)
                            }
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
                            anchors.leftMargin: 4
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
                            anchors.leftMargin: root.rankHeaderLabelLeftInset
                            anchors.rightMargin: root.rankHeaderLabelRightInset
                            visible: !headerCell.editingName
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            font.bold: true
                            font.italic: headerCell.headerName === ""
                            text: root.rankHeaderText(headerCell.headerName, headerCell.headerIssues.length > 0)
                        }
                    }
                }

                // Right after the last rank when they all fit, at the end of the header otherwise.
                Item {
                    id: rankSlot
                    Layout.preferredWidth: root.rankSlotWidth
                    Layout.minimumWidth: root.rankSlotWidth
                    Layout.preferredHeight: root.headerHeight

                    ToolButton {
                        id: addRankPlus
                        objectName: "addRankPlus"
                        visible: !root.addingRank
                        anchors.fill: parent
                        text: "+"
                        Accessible.name: qsTr("Add rank")
                        HoverHandler { id: addRankHover }
                        ToolTip.visible: addRankHover.hovered
                        ToolTip.delay: Theme.toolTipDelay
                        ToolTip.text: qsTr("Add a rank")
                        onClicked: {
                            root.addingRank = true
                            rankCreationName.forceActiveFocus()
                        }
                    }
                    TextField {
                        id: rankCreationName
                        objectName: "rankCreationName"
                        visible: root.addingRank
                        anchors.fill: parent
                        placeholderText: qsTr("Rank name")
                        Accessible.name: placeholderText
                        onAccepted: {
                            if (text.trim() === "") return
                            if (root.requestCreation({kind: "rank", name: text})) {
                                root.addingRank = false
                                text = ""
                            }
                        }
                        Keys.onEscapePressed: {
                            root.addingRank = false
                            text = ""
                        }
                    }
                }
                Item {
                    visible: !emptyRankHeader.visible
                    Layout.fillWidth: true
                }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    ColumnLayout {
                        id: thresholdBody
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 0

                        TableView {
                            id: thresholdView
                            objectName: "thresholdView"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: root.tableModel
                            selectionModel: selection
                            selectionBehavior: TableView.SelectCells
                            selectionMode: TableView.ExtendedSelection
                            pointerNavigationEnabled: false
                            // The identifying columns are not drawn here: the view starts past them (see
                            // hiddenColumnWidth), and every rank column has the same width.
                            leftMargin: -root.hiddenThresholdWidth
                            columnWidthProvider: (column) => root.thresholdColumnWidth(column)
                            rowHeightProvider: () => root.rowHeight
                            onLayoutChanged: root.restoreRankScrollAfterReset()
                            delegate: TableCell {}
                            ScrollBar.vertical: ScrollBar {}
                            // Always shown while the ranks overflow, under the body rather than over its last
                            // row, so a rank column cut off at the edge is visibly reachable by scrolling.
                            ScrollBar.horizontal: ScrollBar {
                                id: thresholdHorizontalBar
                                objectName: "thresholdHorizontalBar"
                                parent: thresholdBody
                                Layout.fillWidth: true
                                policy: ScrollBar.AlwaysOn
                                visible: size < 1
                                implicitHeight: 12
                                // A track and a lighter thumb: the style's default is too faint
                                // against the table to read as "there is more to the right".
                                background: Rectangle {
                                    color: Qt.alpha(thresholdHorizontalBar.palette.windowText, 0.08)
                                }
                                contentItem: Rectangle {
                                    implicitHeight: 8
                                    radius: 4
                                    color: Qt.alpha(thresholdHorizontalBar.palette.windowText,
                                                    thresholdHorizontalBar.pressed ? 0.8
                                                    : thresholdHorizontalBar.hovered ? 0.65 : 0.45)
                                }
                            }

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

                    // Matches the add-rank slot above, so header and body end together.
                    Item {
                        Layout.preferredWidth: root.rankSlotWidth
                        Layout.fillHeight: true
                    }
                }
            }
        }

        // The controls that extend the table. They sit right under its last row (the table above
        // is capped at its content), each with a visible label and a tooltip.
        Row {
            Layout.fillWidth: true
            Layout.preferredHeight: root.rowHeight
            spacing: 0

            AddRowButton {
                id: addCategoryPlus
                objectName: "addCategoryPlus"
                visible: !root.addingCategory
                width: root.identityWidths[0]
                height: root.rowHeight
                text: qsTr("+ Add category")
                Accessible.name: qsTr("Add category")
                ToolTip.text: qsTr("Add a category to group scenarios")
                onClicked: {
                    root.addingCategory = true
                    categoryCreationName.forceActiveFocus()
                }
            }
            TextField {
                id: categoryCreationName
                objectName: "categoryCreationName"
                visible: root.addingCategory
                width: root.identityWidths[0]
                height: root.rowHeight
                placeholderText: qsTr("Category name")
                Accessible.name: placeholderText
                onAccepted: {
                    if (text.trim() === "") return
                    if (root.requestCreation({kind: "category", name: text})) {
                        root.addingCategory = false
                        text = ""
                    }
                }
                Keys.onEscapePressed: {
                    root.addingCategory = false
                    text = ""
                }
            }
            Item { width: root.identityWidths[1]; height: root.rowHeight }
            AddRowButton {
                id: addScenarioPlus
                objectName: "addScenarioPlus"
                width: root.identityWidths[2]
                height: root.rowHeight
                text: qsTr("+ Add scenario")
                Accessible.name: qsTr("Add scenario to Uncategorized")
                ToolTip.text: qsTr("Add a scenario to Uncategorized")
                onClicked: root.addScenarioRequested()
            }
        }

        // Takes the space the table leaves, so the controls above stay under the last row.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Rectangle {
            id: uncategorizedDropZone
            objectName: "uncategorizedDropZone"
            visible: root.draggingRows
            Layout.fillWidth: true
            Layout.preferredHeight: root.rowHeight
            color: Qt.alpha(root.palette.highlight, 0.2)
            border.color: root.hoverDropGroupId === "__uncategorized__" ? root.palette.highlight : root.palette.mid
            Label {
                anchors.centerIn: parent
                text: qsTr("Move to Uncategorized")
            }
        }
    }
}
