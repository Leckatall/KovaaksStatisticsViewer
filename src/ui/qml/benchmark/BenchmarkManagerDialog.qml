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
    readonly property alias assignMenu: assignMenu
    readonly property alias rowContextMenu: rowContextMenu
    readonly property alias groupContextMenu: groupContextMenu
    readonly property alias rankContextMenu: rankContextMenu
    readonly property alias subcategoryRelocationPrompt: subcategoryRelocationPrompt
    readonly property alias editorTable: editorTable

    property string selectedEntryFilename: ""
    // Hides the Library list while a benchmark is open, so the table gets the whole window.
    property bool libraryCollapsed: false
    property string statusMessage: ""
    property bool statusIsError: false
    property string importStatus: ""
    property string tableStatus: ""
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
        if (benchmarkNameField.activeFocus && benchmarkNameField.text !== root.vm().benchmarkName)
            root.vm().setBenchmarkName(benchmarkNameField.text)
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

    function selectedRow() {
        const entries = root.vm().libraryEntries
        if (!root.selectedEntryFilename || !entries) return null
        return entries.find(entry => entry.filename === root.selectedEntryFilename) || null
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
        libraryList.positionViewAtIndex(index, ListView.Contain)
        libraryList.forceActiveFocus()
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

    function matchStateLabel(state) {
        return ({
            resolved: qsTr("Resolved"),
            mappedUnavailable: qsTr("Mapped, unavailable"),
            unresolved: qsTr("Unresolved"),
            ambiguous: qsTr("Ambiguous"),
            autoMappable: qsTr("Pending automatic mapping")
        })[state] || qsTr("Unresolved")
    }

    function offersMappingEdit(state) {
        return state === "resolved" || state === "mappedUnavailable"
    }

    function candidateText(candidate) {
        const parts = [String(candidate.hash),
                       qsTr("%n run(s)", "", candidate.runCount || 0)]
        const played = candidate.lastPlayed
        if (played && !isNaN(new Date(played).getTime()))
            parts.push(Qt.formatDate(played, "yyyy-MM-dd"))
        return parts.join(" · ")
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

    function classificationColor(classification) {
        return ({
            Trackable: "#4CAF50",
            Incomplete: "#FFA726",
            Invalid: "#E53935",
            Unsupported: "#9E9E9E"
        })[classification] || "#9E9E9E"
    }

    function runTableCommand(command) {
        root.commitActiveEdits()
        const result = command()
        root.tableStatus = result && result.ok === false ? result.error : ""
        root.sessionRevision++
        return result
    }

    // ---- Row, rank and group commands ------------------------------------------------------------

    function selectedEntryIds() {
        const ids = root.vm().selectedEntryIds || []
        if (ids.length > 0) return ids.slice()
        return root.currentEntryId !== "" ? [root.currentEntryId] : []
    }

    function assignSelection(targetGroupId) {
        root.commitActiveEdits()
        const ids = root.selectedEntryIds()
        if (ids.length === 0) return
        root.runTableCommand(() => root.vm().assignScenarios(ids, targetGroupId))
    }

    function assignTargets() {
        return root.vm().assignTargets || []
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

    // A category's first subcategory under existing direct scenarios needs the user to say where
    // those scenarios go; the manager never reorganizes them invisibly.
    function requestSubcategory(categoryId, name) {
        root.commitActiveEdits()
        if (name.trim() === "") return false
        const category = (root.vm().groups || []).find(g => g.id === categoryId)
        if (category && category.subcategoryNeedsRelocation === true) {
            subcategoryRelocationPrompt.categoryId = categoryId
            subcategoryRelocationPrompt.subcategoryName = name
            subcategoryRelocationPrompt.open()
            return true
        }
        return root.runTableCommand(() => root.vm().addSubcategory(categoryId, name)).ok
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

    Dialog {
        id: subcategoryRelocationPrompt
        objectName: "subcategoryRelocationPrompt"
        title: qsTr("Add a subcategory")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        property string categoryId
        property string subcategoryName
        // The group the table focuses once the prompt has closed: the new subcategory when one
        // was created, otherwise (Cancel, Escape, a failed command) the category it was asked for.
        property string focusGroupAfterClose

        function relocate(choice) {
            const categoryId = subcategoryRelocationPrompt.categoryId
            const name = subcategoryRelocationPrompt.subcategoryName
            const result = root.runTableCommand(() => root.vm().addSubcategoryRelocating(categoryId, name, choice))
            if (result && result.ok && result.createdId) subcategoryRelocationPrompt.focusGroupAfterClose = result.createdId
            subcategoryRelocationPrompt.close()
        }

        onAboutToShow: subcategoryRelocationPrompt.focusGroupAfterClose = subcategoryRelocationPrompt.categoryId
        // Closing hands focus back to the table, which held it when the prompt opened; once the
        // popup has finished restoring it, the table moves its current cell to that group.
        onClosed: Qt.callLater(editorTable.focusCell, {groupId: subcategoryRelocationPrompt.focusGroupAfterClose})

        ColumnLayout {
            spacing: 8
            Label {
                Layout.maximumWidth: 420
                wrapMode: Text.WordWrap
                text: qsTr("This category already holds scenarios directly. A category cannot hold both "
                           + "scenarios and subcategories, so choose where its scenarios go.")
            }
            Button {
                objectName: "relocateToNewSubcategory"
                text: qsTr("Move them into \"%1\"").arg(subcategoryRelocationPrompt.subcategoryName)
                onClicked: subcategoryRelocationPrompt.relocate("newSubcategory")
            }
            Button {
                objectName: "relocateToUncategorized"
                text: qsTr("Move them to Uncategorized")
                onClicked: subcategoryRelocationPrompt.relocate("uncategorized")
            }
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
        Menu {
            id: assignMenu
            objectName: "assignMenu"
            title: qsTr("Move to group")
            Instantiator {
                model: root.assignTargets()
                delegate: MenuItem {
                    required property var modelData
                    objectName: "assignTarget_" + modelData.id
                    text: modelData.label
                    onTriggered: root.assignSelection(modelData.id)
                }
                onObjectAdded: (index, object) => assignMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => assignMenu.removeItem(object)
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
        MenuItem { text: qsTr("Remove selected scenarios"); onTriggered: root.removeSelectedScenarios() }
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
    readonly property int libraryDefaultWidth: Math.max(libraryToolbar.implicitWidth, 280)

    // A toolbar button sized to its content (the style's 80px minimum would not fit the row). An
    // icon-only button shows its tooltip on hover.
    component ToolbarButton: Button {
        id: toolbarButton
        leftPadding: 8
        rightPadding: 8
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        HoverHandler { id: toolbarHover }
        ToolTip.visible: toolbarHover.hovered && ToolTip.text !== ""
        ToolTip.delay: 400
    }

    // A text field that shows the model value whenever it is not being edited; typing into a
    // TextField destroys a plain `text:` binding, and these fields outlive the element they edit.
    component BoundField: TextField {
        id: boundField
        property string boundText
        selectByMouse: true
        Binding {
            target: boundField
            property: "text"
            value: boundField.boundText
            when: !boundField.activeFocus
            restoreMode: Binding.RestoreNone
        }
    }

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
            ColumnLayout {
                id: libraryPane
                objectName: "libraryPane"
                // Only while an editor is there to take the room: with no draft the list is all
                // the dialog has.
                visible: !root.libraryCollapsed || !root.vm().hasDraft
                SplitView.preferredWidth: root.libraryDefaultWidth
                SplitView.minimumWidth: libraryToolbar.implicitWidth
                spacing: 8

                Label {
                    text: qsTr("Library")
                    font.pixelSize: 18
                    font.bold: true
                }

                // One row at every width the pane can take: the buttons size to their text, and
                // the pane never narrows below the row.
                RowLayout {
                    id: libraryToolbar
                    objectName: "libraryToolbar"
                    Layout.fillWidth: true
                    spacing: 4

                    ToolbarButton {
                        objectName: "newBenchmarkButton"
                        text: qsTr("New")
                        onClicked: root.runWhenClean(() => root.vm().beginNewBenchmark())
                    }
                    ToolbarButton {
                        objectName: "importPlaylistButton"
                        text: qsTr("Import...")
                        Accessible.name: qsTr("Import playlist")
                        ToolTip.text: qsTr("Import a playlist file as a benchmark")
                        onClicked: importDialog.open()
                    }
                    ToolbarButton {
                        objectName: "openBenchmarkButton"
                        text: qsTr("Open")
                        enabled: root.selectedRow() !== null && root.selectedRow().openable
                        onClicked: root.openLibraryEntry(root.selectedRow())
                    }
                    ToolbarButton {
                        objectName: "deleteBenchmarkButton"
                        text: qsTr("Delete")
                        enabled: root.selectedRow() !== null && root.selectedRow().deletable
                        onClicked: {
                            deleteConfirmPrompt.targetId = root.selectedRow().id
                            deleteConfirmPrompt.targetName = root.selectedRow().name
                            deleteConfirmPrompt.open()
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ToolbarButton {
                        objectName: "refreshLibraryButton"
                        icon.source: "../icons/refresh-cw.svg"
                        display: AbstractButton.IconOnly
                        enabled: !root.vm().dirty
                        Accessible.name: qsTr("Refresh")
                        ToolTip.text: qsTr("Refresh the list from the benchmark directory")
                        onClicked: root.vm().refresh()
                    }
                    ToolbarButton {
                        objectName: "openDirectoryButton"
                        icon.source: "../icons/folder-open.svg"
                        display: AbstractButton.IconOnly
                        Accessible.name: qsTr("Open directory")
                        ToolTip.text: qsTr("Open the benchmark directory in the file manager")
                        onClicked:
                            Qt.openUrlExternally(encodeURI("file:///" + root.vm().managedDirectoryPath))
                    }
                }

                Label {
                    objectName: "importStatusLabel"
                    visible: root.importStatus !== ""
                    text: root.importStatus
                    color: root.importStatus.toLowerCase().indexOf("imported") === 0
                        ? root.palette.windowText : "#E57373"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    objectName: "refreshFailedLabel"
                    visible: root.vm().refreshFailed
                    text: qsTr("Last refresh failed: the directory could not be read.")
                    color: "#E57373"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    objectName: "saveStatusLabel"
                    visible: root.statusMessage !== ""
                    text: root.statusMessage
                    color: root.statusIsError ? "#E57373" : root.palette.windowText
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    id: libraryList
                    objectName: "libraryList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.vm().libraryEntries
                    // The selection (selectedEntryFilename) is the single source of truth, so the
                    // arrow keys move it directly instead of a separate current index.
                    keyNavigationEnabled: false
                    activeFocusOnTab: true
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Benchmark library")

                    function step(delta) {
                        const entries = root.vm().libraryEntries || []
                        if (entries.length === 0) return
                        let index = entries.findIndex(entry => entry.filename === root.selectedEntryFilename)
                        index = index < 0 ? (delta > 0 ? 0 : entries.length - 1)
                                          : Math.max(0, Math.min(entries.length - 1, index + delta))
                        root.selectedEntryFilename = entries[index].filename
                        libraryList.positionViewAtIndex(index, ListView.Contain)
                    }
                    Keys.onDownPressed: libraryList.step(1)
                    Keys.onUpPressed: libraryList.step(-1)
                    // Enter opens the selected benchmark, exactly like the Open button.
                    Keys.onReturnPressed: root.openLibraryEntry(root.selectedRow())
                    Keys.onEnterPressed: root.openLibraryEntry(root.selectedRow())

                    delegate: ItemDelegate {
                        id: libraryEntry
                        required property var modelData
                        required property int index
                        width: libraryList.width
                        objectName: "libraryEntry_" + index
                        // The list owns the keyboard focus; the rows only select.
                        focusPolicy: Qt.NoFocus
                        highlighted: root.selectedEntryFilename === libraryEntry.modelData.filename
                        onClicked: {
                            root.selectedEntryFilename = libraryEntry.modelData.filename
                            libraryList.forceActiveFocus()
                        }
                        // Double-click opens through the same guard as the Open button.
                        onDoubleClicked: {
                            root.selectedEntryFilename = libraryEntry.modelData.filename
                            libraryList.forceActiveFocus()
                            root.openLibraryEntry(libraryEntry.modelData)
                        }
                        contentItem: RowLayout {
                            spacing: 8
                            Label {
                                text: libraryEntry.modelData.name
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: root.classificationColor(libraryEntry.modelData.classification)
                            }
                            Label {
                                text: libraryEntry.modelData.classification
                                color: libraryEntry.palette.placeholderText
                            }
                        }
                    }
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

                    // The actions sit beside the name while the name can keep a comfortable width,
                    // and otherwise move to their own row together: never some on each row. The
                    // actions row has a fixed implicit width, so the switch cannot oscillate.
                    GridLayout {
                        id: editorToolbar
                        objectName: "editorToolbar"
                        readonly property real inlineNameWidth: 240
                        readonly property bool actionsInline: editorPane.width
                            >= libraryToggle.implicitWidth + nameRow.spacing + inlineNameWidth
                               + columnSpacing + editorActions.implicitWidth
                        Layout.fillWidth: true
                        columns: actionsInline ? 2 : 1
                        columnSpacing: 8
                        rowSpacing: 6
                        RowLayout {
                            id: nameRow
                            Layout.fillWidth: true
                            spacing: 8
                            ToolbarButton {
                                id: libraryToggle
                                objectName: "libraryToggleButton"
                                icon.source: root.libraryCollapsed ? "../icons/panel-left-open.svg"
                                                                   : "../icons/panel-left-close.svg"
                                display: AbstractButton.IconOnly
                                Accessible.name: root.libraryCollapsed ? qsTr("Show the library list")
                                                                       : qsTr("Hide the library list")
                                ToolTip.text: root.libraryCollapsed
                                    ? qsTr("Show the library list")
                                    : qsTr("Hide the library list to give the table the full width")
                                onClicked: root.libraryCollapsed = !root.libraryCollapsed
                            }
                            BoundField {
                                id: benchmarkNameField
                                objectName: "benchmarkNameField"
                                boundText: root.vm().benchmarkName
                                placeholderText: qsTr("Benchmark name")
                                Layout.fillWidth: true
                                Layout.minimumWidth: 160
                                onEditingFinished: root.vm().setBenchmarkName(text)
                            }
                            // The view model's own dirty state: it compares the draft with the copy it
                            // was opened from or saved to, so undoing back to that copy clears it.
                            Label {
                                objectName: "unsavedLabel"
                                visible: root.vm().dirty === true
                                text: qsTr("• Unsaved")
                                color: Qt.alpha(root.palette.accent, 0.8)
                                Accessible.name: qsTr("The benchmark has unsaved changes")
                            }
                        }
                        RowLayout {
                            id: editorActions
                            objectName: "editorActions"
                            // Inline it sits at the right end of the name row; on its own row it
                            // stays right-aligned under the name.
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            spacing: 4
                            Label {
                                objectName: "draftStatusBadge"
                                Layout.rightMargin: 2
                                text: root.vm().draftTrackable ? qsTr("Trackable") : qsTr("Incomplete")
                            }
                            Rectangle {
                                Layout.rightMargin: 8
                                implicitWidth: 8
                                implicitHeight: 8
                                radius: 4
                                color: root.classificationColor(root.vm().draftTrackable
                                    ? "Trackable" : "Incomplete")
                            }
                            ToolbarButton {
                                objectName: "undoButton"
                                icon.source: "../icons/undo-2.svg"
                                display: AbstractButton.IconOnly
                                enabled: root.sessionRevision >= 0 && root.vm().canUndo === true
                                Accessible.name: qsTr("Undo the last table edit")
                                ToolTip.text: qsTr("Undo the last table edit")
                                onClicked: {
                                    root.commitActiveEdits()
                                    root.tableStatus = ""
                                    editorTable.undo()
                                    root.sessionRevision++
                                }
                            }
                            ToolbarButton {
                                objectName: "pasteButton"
                                icon.source: "../icons/clipboard-paste.svg"
                                display: AbstractButton.IconOnly
                                Accessible.name: qsTr("Paste spreadsheet cells at the current cell or rank header")
                                ToolTip.text: qsTr("Paste spreadsheet cells at the current cell or rank header")
                                onClicked: {
                                    root.commitActiveEdits()
                                    root.tableStatus = ""
                                    editorTable.pasteFromClipboard()
                                    root.sessionRevision++
                                }
                            }
                            ToolbarButton {
                                objectName: "saveButton"
                                icon.source: "../icons/save.svg"
                                display: AbstractButton.IconOnly
                                Accessible.name: qsTr("Save")
                                ToolTip.text: qsTr("Save the benchmark")
                                onClicked: root.requestSave()
                            }
                        }
                    }

                    // The accepted file this editor was opened from has changed, been removed, or
                    // become unreadable since. The working copy is kept as-is; a later Save will
                    // return a conflict. The editor stays fully usable.
                    Label {
                        objectName: "staleBaselineWarning"
                        visible: root.vm().baselineStale
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#E57373"
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
                        color: "#FFA726"
                        text: qsTr("Not valid numbers; Save stores these thresholds as missing: %1")
                            .arg(root.normalizationSummary())
                    }

                    Label {
                        objectName: "tableStatusLabel"
                        readonly property string message: root.tableStatus !== "" ? root.tableStatus : editorTable.statusText
                        visible: message !== ""
                        text: message
                        color: "#E57373"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    // Replacement drops the previous versions; linking preserves them.
                    Pane {
                        id: mappingPanel
                        objectName: "mappingPanel"
                        Layout.fillWidth: true
                        padding: 4
                        property bool candidatesArmed: false
                        property bool linking: false
                        readonly property string mappingState: root.currentRow && root.currentRow.mappingState ? root.currentRow.mappingState : "unresolved"
                        readonly property var candidates: root.currentRow && root.currentRow.mappingCandidates ? root.currentRow.mappingCandidates : []
                        readonly property var mappedHashes: root.currentRow && root.currentRow.mappedHashes ? root.currentRow.mappedHashes : []
                        readonly property bool armed: mappingPanel.candidatesArmed || mappingPanel.mappingState === "ambiguous"
                        Connections {
                            target: root
                            function onCurrentEntryIdChanged() {
                                mappingPanel.candidatesArmed = false
                                mappingPanel.linking = false
                            }
                        }

                        ColumnLayout {
                            width: parent.width
                            spacing: 2
                            Label {
                                objectName: "mappingHint"
                                Layout.fillWidth: true
                                Layout.preferredHeight: clearMappingButton.implicitHeight
                                visible: root.currentEntryId === ""
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: qsTr("Select a scenario to see its mapping")
                                color: root.palette.placeholderText
                            }
                            // Wraps instead of running past a narrow editor, so the buttons stay reachable.
                            Flow {
                                Layout.fillWidth: true
                                visible: root.currentEntryId !== ""
                                spacing: 8
                                Label {
                                    height: clearMappingButton.implicitHeight
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Mapping for %1:").arg(root.currentRow && root.currentRow.name ? root.currentRow.name : qsTr("(unnamed)"))
                                }
                                Label {
                                    objectName: "matchLabel"
                                    height: clearMappingButton.implicitHeight
                                    verticalAlignment: Text.AlignVCenter
                                    text: root.matchStateLabel(mappingPanel.mappingState)
                                    color: root.palette.placeholderText
                                }
                                Button {
                                    objectName: "changeMappingButton"
                                    text: mappingPanel.mappedHashes.length > 1 ? qsTr("Replace mappings") : qsTr("Change mapping")
                                    visible: root.offersMappingEdit(mappingPanel.mappingState)
                                    Accessible.name: qsTr("Change the scenario mapping")
                                    onClicked: {
                                        mappingPanel.linking = false
                                        mappingPanel.candidatesArmed = !mappingPanel.candidatesArmed
                                    }
                                }
                                Button {
                                    objectName: "linkVersionsButton"
                                    text: qsTr("Link versions")
                                    checkable: true
                                    checked: mappingPanel.linking
                                    enabled: mappingPanel.linking
                                        || mappingPanel.candidates.some(candidate => mappingPanel.mappedHashes.indexOf(candidate.hash) < 0)
                                    Accessible.name: qsTr("Link another version with the same scenario name")
                                    onClicked: {
                                        mappingPanel.linking = !mappingPanel.linking
                                        mappingPanel.candidatesArmed = false
                                    }
                                }
                                Button {
                                    id: clearMappingButton
                                    objectName: "clearMappingButton"
                                    text: qsTr("Clear mapping")
                                    visible: root.offersMappingEdit(mappingPanel.mappingState)
                                    Accessible.name: qsTr("Clear the scenario mapping")
                                    onClicked: root.requestMappingEdit(root.currentEntryId, "")
                                }
                            }
                            Repeater {
                                model: mappingPanel.mappedHashes
                                delegate: RowLayout {
                                    id: linkedVersion
                                    required property string modelData
                                    Layout.fillWidth: true
                                    readonly property var candidate: mappingPanel.candidates.find(value => value.hash === linkedVersion.modelData)
                                    readonly property bool available: linkedVersion.candidate !== undefined
                                        || (root.vm().scenarioCatalogue || []).some(value => value.hash === linkedVersion.modelData)
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                        text: linkedVersion.candidate ? root.candidateText(linkedVersion.candidate)
                                            : (linkedVersion.available ? linkedVersion.modelData
                                                : qsTr("%1 · unavailable").arg(linkedVersion.modelData))
                                    }
                                    Button {
                                        objectName: "unlinkVersion_" + linkedVersion.modelData
                                        text: qsTr("Unlink")
                                        Accessible.name: qsTr("Unlink scenario version %1").arg(linkedVersion.modelData)
                                        onClicked: root.requestLinkedVersion(linkedVersion.modelData, false)
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: mappingPanel.linking
                                text: qsTr("Choose a version to include its scores and history in this benchmark row.")
                                wrapMode: Text.WordWrap
                                color: root.palette.placeholderText
                            }
                            Repeater {
                                model: root.currentEntryId !== "" ? mappingPanel.candidates : []
                                delegate: ItemDelegate {
                                    id: candidateOption
                                    required property var modelData
                                    objectName: "candidateOption_" + candidateOption.modelData.hash
                                    Layout.fillWidth: true
                                    visible: mappingPanel.candidatesArmed
                                        || mappingPanel.mappedHashes.indexOf(candidateOption.modelData.hash) < 0
                                    enabled: mappingPanel.linking
                                        ? mappingPanel.mappedHashes.indexOf(candidateOption.modelData.hash) < 0
                                        : mappingPanel.armed
                                    opacity: enabled ? 1 : 0.4
                                    text: root.candidateText(candidateOption.modelData)
                                    Accessible.name: mappingPanel.linking
                                        ? qsTr("Link version %1").arg(candidateOption.text)
                                        : qsTr("Map to %1").arg(candidateOption.text)
                                    onClicked: {
                                        if (mappingPanel.linking) root.requestLinkedVersion(candidateOption.modelData.hash, true)
                                        else root.requestMappingEdit(root.currentEntryId, candidateOption.modelData.hash)
                                    }
                                }
                            }
                        }
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
                            case "category": return root.runTableCommand(() => root.vm().addCategory(request.name)).ok
                            case "subcategory": return root.requestSubcategory(request.categoryId, request.name)
                            case "rank": return root.runTableCommand(() => root.vm().addTier(request.name)).ok
                            }
                            return false
                        }
                        onMoveSelectionRequested: (entryIds, targetId, beforeId) => root.runTableCommand(
                            () => root.vm().assignScenarios(entryIds, targetId, beforeId))
                        onScenarioIdentityRequested: (entryId, name, hash) => root.requestScenarioIdentity(entryId, name, hash)
                        onRenameGroupRequested: (groupId, name) => root.runTableCommand(
                            () => root.vm().renameGroup(groupId, name))
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
}
