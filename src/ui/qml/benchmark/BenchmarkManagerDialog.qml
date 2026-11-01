import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import KovaaksStatsViewer

ApplicationWindow {
    id: root
    objectName: "benchmarkManagerDialog"
    title: qsTr("Benchmark Manager")
    flags: Qt.Dialog
    modality: Qt.WindowModal
    palette: Theme.palette
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 560

    required property var benchmarkManagerVm
    // Every command goes through the table's handle on the view model. For the C++ view model this
    // is the same object; a plain-JS test double is copied whenever it is assigned to a property,
    // so the dialog reads the table's copy through a function rather than a property of its own.
    function vm() { return editorTable.manager }

    readonly property alias dirtyClosePrompt: dirtyClosePrompt
    readonly property alias deleteConfirmPrompt: deleteConfirmPrompt
    readonly property alias colorDialog: colorDialog
    readonly property alias rowContextMenu: rowContextMenu
    readonly property alias groupContextMenu: groupContextMenu
    readonly property alias rankContextMenu: rankContextMenu
    readonly property alias editorTable: editorTable
    readonly property alias groupCommand: groupCommand

    property string selectedEntryFilename: ""
    // Hides the Library list while a benchmark is open, so the table gets the whole window.
    property bool libraryCollapsed: false
    property string statusMessage: ""
    property bool statusIsError: false
    property string importStatus: ""
    property string tableStatus: ""
    // A tableStatus that reports success rather than a problem.
    property bool tableStatusIsNotice: false
    property var pendingAction: null
    // Bumped after every command and session publication, so bindings over view-model state that
    // is not itself observable (the plain-JS test double) re-read it.
    property int sessionRevision: 0

    // ---- Table context -----------------------------------------------------------------------
    readonly property string currentTierId: editorTable.focusedTierId
    readonly property string currentGroupId: editorTable.focusedGroupId
    readonly property var currentRow: editorTable.currentRowInfo
    readonly property string currentEntryId: root.currentRow && root.currentRow.entryId ? root.currentRow.entryId : ""
    readonly property var currentTier: (root.vm().tiers || []).find(t => t.id === root.currentTierId) || null
    readonly property int currentTierIndex: (root.vm().tiers || []).findIndex(t => t.id === root.currentTierId)
    readonly property var currentGroup: (root.vm().groups || []).find(g => g.id === root.currentGroupId) || null

    Connections {
        target: typeof root.vm().draftChanged === "function" ? root.vm() : null
        function onDraftChanged() { root.sessionRevision++ }
    }

    // Flushes every open text edit into the session: the table cell editor and any name field
    // still holding typed text. Save, dirty checks and structural commands call this first, so
    // they act on what the user typed even though no editingFinished has fired.
    function commitActiveEdits() {
        editorTable.commitActiveEdit()
        const nameField = editorToolbar.nameField
        if (nameField.activeFocus && nameField.text !== root.vm().benchmarkName)
            root.vm().setBenchmarkName(nameField.text)
        root.sessionRevision++
    }

    function normalizationIssues() {
        return root.sessionRevision >= 0 ? (root.vm().normalizationIssues || []) : []
    }

    // "“12oops” (Scenario / Rank)" for each retained input a successful Save stores as missing.
    function normalizationSummary() {
        const model = root.vm().tableModel
        const tiers = root.vm().tiers || []
        return root.normalizationIssues().map(issue => {
            const row = model ? model.rowForEntry(issue.entryId) : -1
            const scenario = row >= 0 ? model.rowInfo(row).name : ""
            const tier = tiers.find(t => t.id === issue.tierId)
            return "\u201C" + issue.text + "\u201D (" + (scenario !== "" ? scenario : qsTr("unnamed scenario"))
                + " / " + (tier && tier.name !== "" ? tier.name : qsTr("unnamed rank")) + ")"
        }).join(", ")
    }

    // Opens a library entry in the editor. Double-click, Enter and the Open button all come here,
    // so each passes through the same unsaved-changes guard; entries that cannot be opened do
    // nothing.
    function openLibraryEntry(entry) {
        if (!entry || !entry.openable) return
        root.runWhenClean(() => root.vm().openBenchmark(entry.id))
    }

    function open() {
        root.selectedEntryFilename = ""
        root.libraryCollapsed = false
        root.statusMessage = ""
        root.statusIsError = false
        root.importStatus = ""
        root.tableStatus = ""
        root.pendingAction = null
        visible = true
        raise()
        requestActivate()
    }

    // Opens the manager on one library entry: selected in the library, and opened in the editor
    // when it can be (through the same unsaved-changes guard as Open). A problem file can have an
    // empty id, so the filename identifies the entry; the id is the fallback when no filename is
    // given. A draft of the same benchmark already in the editor is kept, edits and all.
    function openOnEntry(filename, benchmarkId) {
        root.open()
        const entries = root.vm().libraryEntries || []
        let index = filename ? entries.findIndex(entry => entry.filename === filename) : -1
        if (index < 0 && benchmarkId)
            index = entries.findIndex(entry => entry.id === benchmarkId)
        if (index < 0) return
        const entry = entries[index]
        root.selectedEntryFilename = entry.filename
        libraryPane.revealEntry(index)
        const alreadyOpen = root.vm().hasDraft && root.vm().draftFromLibrary && root.vm().draftId === entry.id
        if (!alreadyOpen) root.openLibraryEntry(entry)
    }

    onClosing: (close) => {
        root.commitActiveEdits()
        if (root.vm().dirty) {
            close.accepted = false
            root.runWhenClean(() => root.closeWindow())
            return
        }
        root.closeDraft()
    }

    function closeWindow() {
        root.closeDraft()
        root.visible = false
    }

    // The open benchmark is held back from automatic linking to your runs until the editor
    // closes it, so the window never keeps one open behind it.
    function closeDraft() {
        if (root.vm().hasDraft) root.vm().discard()
    }

    // Every draft-replacing transition (New / Open / Import / close) waits for an explicit
    // Save/Discard/Cancel decision while a dirty draft exists, so unfinished work can never
    // be silently dropped.
    function runWhenClean(action) {
        root.commitActiveEdits()
        if (!root.vm().dirty) {
            action()
            return
        }
        root.pendingAction = action
        dirtyClosePrompt.open()
    }

    function runPendingAction() {
        const action = root.pendingAction
        root.pendingAction = null
        if (action) action()
    }

    // A deferred action belongs to the transition that armed it: a save that fails abandons that
    // transition, so the action must be dropped rather than left to fire on a later save.
    function doSave() {
        const result = root.vm().save()
        root.sessionRevision++
        root.statusMessage = result.ok ? qsTr("Saved.") : result.error
        root.statusIsError = !result.ok
        if (result.ok) runPendingAction()
        else root.pendingAction = null
        return result.ok
    }

    function requestSave() {
        root.commitActiveEdits()
        root.doSave()
    }

    function requestMappingEdit(entryId, hash) {
        root.commitActiveEdits()
        root.vm().setScenarioHash(entryId, hash)
    }

    function requestScenarioIdentity(entryId, name, hash) {
        root.runTableCommand(() => root.vm().setScenarioIdentity(entryId, name, hash))
    }

    function requestLinkedVersion(hash, linked) {
        const entryId = root.currentEntryId
        const current = root.currentRow && root.currentRow.mappedHashes ? root.currentRow.mappedHashes : []
        const hashes = Array.from(current).filter(value => value !== hash)
        if (linked) hashes.push(hash)
        root.runTableCommand(() => root.vm().setScenarioHashes(entryId, hashes))
    }

    function importFromUrl(url) {
        root.runWhenClean(() => {
            const result = root.vm().importPlaylist(url)
            root.importStatus = result.ok
                ? (result.skipped.length > 0
                    ? qsTr("Imported — skipped %n duplicate(s).", "", result.skipped.length)
                    : qsTr("Imported."))
                : result.error
        })
    }

    function runTableCommand(command) {
        root.commitActiveEdits()
        const result = command()
        root.tableStatus = result && result.ok === false ? result.error : ""
        root.tableStatusIsNotice = false
        root.sessionRevision++
        return result
    }

    function showNotice(text) {
        root.tableStatus = text
        root.tableStatusIsNotice = true
    }

    function groupName(id) {
        const group = (root.vm().groups || []).find(g => g.id === id)
        return group ? group.name : ""
    }

    // "Parent › Child" for a subcategory, matching the Ctrl+G list's paths.
    function groupPath(id) {
        if (id === "") return qsTr("Uncategorized")
        const group = (root.vm().groups || []).find(g => g.id === id)
        if (!group) return ""
        return group.kind === "subcategory" ? root.groupName(group.parentId) + " › " + group.name : group.name
    }

    function requestCategories(text) {
        const result = root.runTableCommand(() => root.vm().addCategories(text))
        if (!result.ok) return false
        const names = result.createdIds.map(root.groupName).filter(name => name !== "")
        const count = result.createdIds.length
        let message = count === 0 ? qsTr("No categories added.")
                    : count === 1 ? qsTr("Added 1 empty category")
                    : qsTr("Added %1 empty categories").arg(count)
        if (count > 0) message += names.length > 0 ? ": " + names.join(", ") + "." : "."
        if (result.skipped.length > 0)
            message += " " + qsTr("Skipped: %1.").arg(result.skipped.join(", "))
        root.showNotice(message)
        return true
    }

    // ---- Group command (Ctrl+G) ---------------------------------------------------------------

    function openGroupCommand() {
        root.commitActiveEdits()
        const ids = root.selectedEntryIds()
        if (ids.length === 0) {
            root.tableStatus = qsTr("Select one or more scenarios first.")
            root.tableStatusIsNotice = false
            return
        }
        const model = root.vm().tableModel
        groupCommand.scenarioNames = ids
            .map(id => model.rowInfo(model.rowForEntry(id)))
            .filter(info => info.row !== undefined)
            .sort((a, b) => a.row - b.row)
            .map(info => info.name)
        groupCommand.parentCategories = (root.vm().groups || [])
            .filter(group => group.kind === "category")
            .map(group => ({id: group.id, name: group.name}))
        groupCommand.open()
    }

    // Focus goes back through the session's own selection: focusCell() would collapse it to the
    // current cell.
    function returnFocusToTable() {
        Qt.callLater(() => {
            editorTable.restoreSessionSelection()
            editorTable.revealCurrent()
        })
    }

    function createGroupAndMove(name, parentCategoryId) {
        const ids = root.selectedEntryIds()
        const result = root.runTableCommand(() => root.vm().createGroupAndAssign(ids, name, parentCategoryId))
        if (result.ok) {
            const single = ids.length === 1
            root.showNotice(parentCategoryId === ""
                ? (single ? qsTr("Created “%1” and moved 1 scenario.").arg(name)
                          : qsTr("Created “%1” and moved %2 scenarios.").arg(name).arg(ids.length))
                : (single ? qsTr("Created “%1” under %2 and moved 1 scenario.").arg(name).arg(root.groupName(parentCategoryId))
                          : qsTr("Created “%1” under %2 and moved %3 scenarios.").arg(name)
                                .arg(root.groupName(parentCategoryId)).arg(ids.length)))
        }
        root.returnFocusToTable()
    }

    function moveToGroup(targetGroupId) {
        const ids = root.selectedEntryIds()
        const result = root.runTableCommand(() => root.vm().assignScenarios(ids, targetGroupId))
        if (result.ok) {
            const destination = root.groupPath(targetGroupId)
            root.showNotice(ids.length === 1 ? qsTr("Moved 1 scenario to %1.").arg(destination)
                                             : qsTr("Moved %1 scenarios to %2.").arg(ids.length).arg(destination))
        }
        root.returnFocusToTable()
    }

    // ---- Row, rank and group commands ------------------------------------------------------------

    function selectedEntryIds() {
        const ids = root.vm().selectedEntryIds || []
        if (ids.length > 0) return ids.slice()
        return root.currentEntryId !== "" ? [root.currentEntryId] : []
    }

    // Position and size of the current row within the collection that holds it: a subcategory,
    // a category's direct scenarios (which precede its subcategories), or Uncategorized.
    function currentRowPlacement() {
        const info = root.currentRow
        if (!info || info.row === undefined) return {position: -1, size: 0}
        return {position: info.positionInContainer, size: info.containerSize}
    }

    function moveCurrentRow(delta) {
        const placement = root.currentRowPlacement()
        const target = placement.position + delta
        if (placement.position < 0 || target < 0 || target >= placement.size) return
        root.runTableCommand(() => root.vm().reorderScenario(root.currentEntryId, target))
    }

    // One command for the whole selection, so one Undo restores every removed row.
    function removeSelectedScenarios() {
        root.commitActiveEdits()
        const ids = root.selectedEntryIds()
        if (ids.length > 0)
            root.runTableCommand(() => root.vm().removeScenarios(ids))
    }

    function moveCurrentRankBy(delta) {
        const target = root.currentTierIndex + delta
        if (root.currentTierIndex < 0 || target < 0 || target >= (root.vm().tiers || []).length) return
        root.runTableCommand(() => root.vm().reorderTier(root.currentTierId, target))
    }

    function removeCurrentRank() {
        if (root.currentTierId === "") return
        root.runTableCommand(() => root.vm().removeTier(root.currentTierId))
    }

    function groupSiblings(group) {
        const groups = root.vm().groups || []
        return group.kind === "category" ? groups.filter(g => g.kind === "category")
                                         : groups.filter(g => g.kind === "subcategory" && g.parentId === group.parentId)
    }

    function currentGroupIndex() {
        return root.currentGroup ? root.groupSiblings(root.currentGroup).findIndex(g => g.id === root.currentGroupId) : -1
    }

    function moveCurrentGroup(delta) {
        const group = root.currentGroup
        if (!group) return
        const target = root.currentGroupIndex() + delta
        if (target < 0 || target >= root.groupSiblings(group).length) return
        root.runTableCommand(() => group.kind === "category"
            ? root.vm().reorderCategory(group.id, target)
            : root.vm().reorderSubcategory(group.id, target))
    }

    function requestSubcategory(categoryId, name) {
        root.commitActiveEdits()
        if (name.trim() === "") return false
        const result = root.runTableCommand(() => root.vm().addSubcategory(categoryId, name))
        if (result.ok) Qt.callLater(editorTable.focusCell, {groupId: result.createdId})
        return result.ok
    }

    function focusValidationIssue(issue) {
        const model = root.vm().tableModel
        if (!model || !issue.targetId) return
        if (issue.tierId) editorTable.focusCell({entryId: issue.targetId, tierId: issue.tierId})
        else if (model.rowForEntry(issue.targetId) >= 0) editorTable.focusCell({entryId: issue.targetId})
        else if (model.columnForTier(issue.targetId) >= 0) editorTable.focusCell({tierId: issue.targetId})
        else editorTable.focusCell({groupId: issue.targetId})
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
    }

    MessageDialog {
        id: dirtyClosePrompt
        objectName: "dirtyClosePrompt"
        title: qsTr("Unsaved changes")
        text: qsTr("You have unsaved changes in the benchmark editor.")
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: function (button) {
            if (button === MessageDialog.Save) {
                root.requestSave()
            } else if (button === MessageDialog.Discard) {
                root.vm().discard()
                root.runPendingAction()
            } else {
                root.pendingAction = null
            }
        }
    }

    MessageDialog {
        id: deleteConfirmPrompt
        objectName: "deleteConfirmPrompt"
        title: qsTr("Delete benchmark?")
        buttons: MessageDialog.Yes | MessageDialog.Cancel
        property string targetId
        property string targetName
        text: qsTr("Delete \"%1\"? Its file will be removed from the managed directory.").arg(targetName)
        onButtonClicked: function (button) {
            if (button !== MessageDialog.Yes) return
            const remove = () => {
                const result = root.vm().deleteBenchmark(deleteConfirmPrompt.targetId)
                root.statusMessage = result.ok ? qsTr("Deleted.") : result.error
                root.statusIsError = !result.ok
                if (result.ok) root.selectedEntryFilename = ""
            }
            // Deleting the benchmark currently being edited discards the draft with it, so it is a
            // draft-replacing transition too; deleting any other entry leaves the draft alone.
            if (deleteConfirmPrompt.targetId === root.vm().draftId) root.runWhenClean(remove)
            else remove()
        }
    }

    FileDialog {
        id: importDialog
        objectName: "importPlaylistDialog"
        title: qsTr("Import playlist")
        fileMode: FileDialog.OpenFile
        nameFilters: ["KovaaKs Playlist (*.json)", "All files (*)"]
        onAccepted: root.importFromUrl(importDialog.selectedFiles[0])
    }

    ColorDialog {
        id: colorDialog
        objectName: "colorDialog"
        property string targetKind
        property string targetId
        onAccepted: {
            if (colorDialog.targetKind === "tier")
                root.vm().setTierColor(colorDialog.targetId, colorDialog.selectedColor)
            else
                root.vm().setGroupColor(colorDialog.targetId, colorDialog.selectedColor)
        }
    }

    Menu {
        id: rowContextMenu
        objectName: "rowContextMenu"
        MenuItem {
            objectName: "groupScenariosItem"
            action: Action {
                text: qsTr("Group Scenarios")
                shortcut: "Ctrl+G"
                enabled: root.vm().hasDraft && !groupCommand.visible
                // The hotkey fires this with the menu still open, and nothing else closes it then.
                onTriggered: {
                    rowContextMenu.dismiss()
                    root.openGroupCommand()
                }
            }
        }
        MenuItem { text: qsTr("Move up"); enabled: root.currentRowPlacement().position > 0; onTriggered: root.moveCurrentRow(-1) }
        MenuItem {
            text: qsTr("Move down")
            readonly property var placement: root.currentRowPlacement()
            enabled: placement.position >= 0 && placement.position < placement.size - 1
            onTriggered: root.moveCurrentRow(1)
        }
        MenuSeparator {}
        MenuItem {
            objectName: "removeScenariosItem"
            action: Action {
                text: qsTr("Remove selected scenarios")
                shortcut: StandardKey.Delete
                // The shortcut is here so the item shows the key. Enabled with the menu closed, it
                // would take Delete from the table, which refuses it on group and rank cells.
                enabled: rowContextMenu.visible
                onTriggered: root.removeSelectedScenarios()
            }
        }
    }

    Menu {
        id: groupContextMenu
        objectName: "groupContextMenu"
        MenuItem {
            text: qsTr("Add subcategory")
            visible: root.currentGroup && root.currentGroup.kind === "category"
            onTriggered: editorTable.beginSubcategoryCreation(root.currentGroupId)
        }
        MenuItem { text: qsTr("Rename"); onTriggered: editorTable.beginGroupRename(root.currentGroupId) }
        MenuItem {
            text: qsTr("Change colour")
            onTriggered: if (root.currentGroup) root.openColorDialog("group", root.currentGroupId, root.currentGroup.color)
        }
        MenuSeparator {}
        MenuItem { text: qsTr("Move up"); enabled: root.currentGroupIndex() > 0; onTriggered: root.moveCurrentGroup(-1) }
        MenuItem {
            text: qsTr("Move down")
            enabled: root.currentGroup && root.currentGroupIndex() < root.groupSiblings(root.currentGroup).length - 1
            onTriggered: root.moveCurrentGroup(1)
        }
        MenuItem {
            text: qsTr("Remove group")
            onTriggered: root.runTableCommand(() => root.vm().removeCategory(root.currentGroupId))
        }
    }

    Menu {
        id: rankContextMenu
        objectName: "rankContextMenu"
        MenuItem { text: qsTr("Rename"); onTriggered: editorTable.beginRankRename(root.currentTierId) }
        MenuItem {
            text: qsTr("Change colour")
            onTriggered: if (root.currentTier) root.openColorDialog("tier", root.currentTierId, root.currentTier.color)
        }
        MenuSeparator {}
        MenuItem { text: qsTr("Move left"); enabled: root.currentTierIndex > 0; onTriggered: root.moveCurrentRankBy(-1) }
        MenuItem {
            text: qsTr("Move right")
            enabled: root.currentTierIndex >= 0 && root.currentTierIndex < (root.vm().tiers || []).length - 1
            onTriggered: root.moveCurrentRankBy(1)
        }
        MenuItem { text: qsTr("Remove rank"); onTriggered: root.removeCurrentRank() }
    }

    function openColorDialog(kind, id, color) {
        colorDialog.targetKind = kind
        colorDialog.targetId = id
        colorDialog.selectedColor = color
        colorDialog.open()
    }

    // The width the Library pane opens at: just the toolbar row, which leaves the rest to the editor.
    readonly property int libraryDefaultWidth: Math.max(libraryPane.toolbarImplicitWidth, 280)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // The Library list and the editor share the window through a draggable divider, so the
        // table can be given the room it needs (every rank column) at the cost of the list.
        SplitView {
            id: paneSplit
            objectName: "paneSplit"
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                id: paneHandle
                implicitWidth: 10
                color: "transparent"
                Accessible.role: Accessible.Splitter
                Accessible.name: qsTr("Resize the library list")

                Rectangle {
                    anchors.centerIn: parent
                    width: SplitHandle.pressed || SplitHandle.hovered ? 3 : 1
                    height: parent.height
                    color: SplitHandle.pressed ? root.palette.highlight
                         : SplitHandle.hovered ? Qt.lighter(root.palette.mid, 1.6) : root.palette.mid
                }
                HoverHandler {
                    cursorShape: Qt.SplitHCursor
                }
            }

            // ---- Library pane ------------------------------------------------
            BenchmarkLibraryPane {
                id: libraryPane
                // Only while an editor is there to take the room: with no draft the list is all
                // the dialog has.
                visible: !root.libraryCollapsed || !root.vm().hasDraft
                SplitView.preferredWidth: root.libraryDefaultWidth
                SplitView.minimumWidth: libraryPane.toolbarImplicitWidth
                manager: root.vm()
                selectedEntryFilename: root.selectedEntryFilename
                importStatus: root.importStatus
                statusMessage: root.statusMessage
                statusIsError: root.statusIsError
                onEntrySelected: filename => root.selectedEntryFilename = filename
                onNewRequested: root.runWhenClean(() => root.vm().beginNewBenchmark())
                onImportRequested: importDialog.open()
                onRefreshRequested: root.vm().refresh()
                onOpenRequested: entry => root.openLibraryEntry(entry)
                onDeleteRequested: entry => {
                    deleteConfirmPrompt.targetId = entry.id
                    deleteConfirmPrompt.targetName = entry.name
                    deleteConfirmPrompt.open()
                }
            }

            // ---- Editor pane ---------------------------------------------------
            Item {
                id: editorPane
                objectName: "editorPane"
                SplitView.fillWidth: true
                // Never so narrow that the table loses its last rank column (and its scrollbar).
                SplitView.minimumWidth: Math.max(360, editorTable.minimumTableWidth)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    visible: root.vm().hasDraft
                    // A closed draft brings the list back, so the next open starts with it shown.
                    onVisibleChanged: if (!visible) root.libraryCollapsed = false

                    BenchmarkEditorToolbar {
                        id: editorToolbar
                        Layout.fillWidth: true
                        manager: root.vm()
                        libraryCollapsed: root.libraryCollapsed
                        sessionRevision: root.sessionRevision
                        availableWidth: editorPane.width
                        onLibraryToggleRequested: root.libraryCollapsed = !root.libraryCollapsed
                        onNameEdited: name => root.vm().setBenchmarkName(name)
                        onUndoRequested: {
                            root.commitActiveEdits()
                            root.tableStatus = ""
                            editorTable.undo()
                            root.sessionRevision++
                        }
                        onPasteRequested: {
                            root.commitActiveEdits()
                            root.tableStatus = ""
                            editorTable.pasteFromClipboard()
                            root.sessionRevision++
                        }
                        onSaveRequested: root.requestSave()
                    }

                    // The accepted file this editor was opened from has changed, been removed, or
                    // become unreadable since. The working copy is kept as-is; a later Save will
                    // return a conflict. The editor stays fully usable.
                    Label {
                        objectName: "staleBaselineWarning"
                        visible: root.vm().baselineStale
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.errorText
                        text: qsTr("The saved benchmark this editor was opened from has changed on disk. "
                                   + "Your edits are kept here; saving will report a conflict until you reopen it.")
                    }

                    // Non-modal: the editor stays fully usable while this is shown. It is bound
                    // straight to the flag, so the next reconciliation that publishes without a
                    // write failure clears it on its own.
                    Rectangle {
                        objectName: "resolutionFailureBanner"
                        visible: root.vm().resolutionWriteFailed
                        Layout.fillWidth: true
                        implicitHeight: resolutionFailureText.implicitHeight + 12
                        radius: 4
                        color: "#5D4037"
                        Label {
                            id: resolutionFailureText
                            objectName: "resolutionFailureBannerLabel"
                            anchors.fill: parent
                            anchors.margins: 6
                            wrapMode: Text.WordWrap
                            color: "#FFFFFF"
                            text: qsTr("Automatic scenario resolution could not be written. Your mappings are unchanged; the manager retries on the next refresh.")
                        }
                    }

                    // What a successful Save will store as missing. Save stays available: this informs
                    // the existing gates rather than adding a compulsory repair step.
                    Label {
                        objectName: "normalizationWarning"
                        visible: root.normalizationIssues().length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.warning
                        text: qsTr("Not valid numbers; Save stores these thresholds as missing: %1")
                            .arg(root.normalizationSummary())
                    }

                    Label {
                        objectName: "tableStatusLabel"
                        readonly property string message: root.tableStatus !== "" ? root.tableStatus : editorTable.statusText
                        visible: message !== ""
                        text: message
                        color: root.tableStatusIsNotice && root.tableStatus !== "" ? root.palette.windowText : Theme.errorText
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    BenchmarkMappingPanel {
                        id: mappingPanel
                        Layout.fillWidth: true
                        currentRow: root.currentRow
                        scenarioCatalogue: root.vm().scenarioCatalogue || []
                        onMappingEditRequested: (entryId, hash) => root.requestMappingEdit(entryId, hash)
                        onLinkedVersionRequested: (hash, linked) => root.requestLinkedVersion(hash, linked)
                    }

                    // ---- Validation -----------------------------------------------------------------
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(validationPanel.implicitHeight, 96)
                        visible: root.vm().validationIssues.length > 0
                        clip: true
                        ColumnLayout {
                            id: validationPanel
                            objectName: "validationPanel"
                            spacing: 0

                            Repeater {
                                model: root.vm().validationIssues
                                delegate: Button {
                                    id: validationIssue
                                    required property var modelData
                                    required property int index
                                    objectName: "validationIssue_" + index
                                    flat: true
                                    Layout.fillWidth: true
                                    Accessible.name: validationIssue.modelData.message
                                    contentItem: Text {
                                        text: "• " + validationIssue.modelData.message
                                        horizontalAlignment: Text.AlignLeft
                                        elide: Text.ElideRight
                                        color: validationIssue.palette.buttonText
                                    }
                                    onClicked: root.focusValidationIssue(validationIssue.modelData)
                                }
                            }
                        }
                    }

                    BenchmarkEditorTable {
                        id: editorTable
                        objectName: "editorTable"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        manager: root.benchmarkManagerVm
                        onColorSwatchRequested: (kind, id, color) => root.openColorDialog(kind, id, color)
                        onAddScenarioRequested: {
                            const result = root.runTableCommand(() => root.vm().addUnplayedScenario(""))
                            if (result && result.ok && result.createdId)
                                Qt.callLater(() => editorTable.beginScenarioEdit(result.createdId))
                        }
                        creationHandler: request => {
                            switch (request.kind) {
                            case "categories": return root.requestCategories(request.text)
                            case "subcategory": return root.requestSubcategory(request.categoryId, request.name)
                            case "rank": return root.runTableCommand(() => root.vm().addTier(request.name)).ok
                            }
                            return false
                        }
                        onMoveSelectionRequested: (entryIds, targetId, beforeId) => root.runTableCommand(
                            () => root.vm().assignScenarios(entryIds, targetId, beforeId))
                        onRemoveScenariosRequested: root.removeSelectedScenarios()
                        onScenarioIdentityRequested: (entryId, name, hash) => root.requestScenarioIdentity(entryId, name, hash)
                        onRowContextRequested: (entryId, x, y) => {
                            rowContextMenu.popup(editorTable, x, y)
                        }
                        onGroupContextRequested: (groupId, x, y) => {
                            groupContextMenu.popup(editorTable, x, y)
                        }
                        onRankContextRequested: (tierId, x, y) => {
                            rankContextMenu.popup(editorTable, x, y)
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 20, 360)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: !root.vm().hasDraft
                    text: qsTr("Create, import, or open a benchmark to edit it.")
                    color: root.palette.placeholderText
                }
            }
        }
    }

    BenchmarkGroupCommand {
        id: groupCommand
        parent: Overlay.overlay
        optionsProvider: (query, parentCategoryId) => root.vm().groupCommandOptions(query, parentCategoryId)
        onCreateRequested: (name, parentCategoryId) => root.createGroupAndMove(name, parentCategoryId)
        onMoveRequested: targetGroupId => root.moveToGroup(targetGroupId)
    }
}
