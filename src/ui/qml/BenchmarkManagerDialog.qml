import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

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
    readonly property alias retrospectivePrompt: retrospectivePrompt
    readonly property alias deleteConfirmPrompt: deleteConfirmPrompt
    readonly property alias colorDialog: colorDialog
    readonly property alias assignMenu: assignMenu
    readonly property alias subcategoryRelocationPrompt: subcategoryRelocationPrompt
    readonly property alias editorTable: editorTable

    property string selectedEntryFilename: ""
    property string statusMessage: ""
    property bool statusIsError: false
    property string importStatus: ""
    property string tableStatus: ""
    property var pendingAction: null
    // Bumped after every command and session publication, so bindings over view-model state that
    // is not itself observable (the plain-JS test double) re-read it.
    property int sessionRevision: 0

    // A mapping edit or a save on an opened (from-library) definition must clear the
    // retrospective reinterpretation warning first; the confirmed action is parked here.
    property var pendingRetrospectiveAction: null
    property string knownScenarioFilter: ""
    property string selectedKnownScenarioHash: ""
    property string selectedKnownScenarioName: ""

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
        if (rankNameField.activeFocus && root.currentTierId !== "" && rankNameField.text !== rankNameField.boundText)
            root.vm().renameTier(root.currentTierId, rankNameField.text)
        if (groupNameField.activeFocus && root.currentGroupId !== "" && groupNameField.text !== groupNameField.boundText)
            root.vm().renameGroup(root.currentGroupId, groupNameField.text)
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

    function open() {
        root.selectedEntryFilename = ""
        root.statusMessage = ""
        root.statusIsError = false
        root.importStatus = ""
        root.tableStatus = ""
        root.pendingAction = null
        visible = true
        raise()
        requestActivate()
    }

    onClosing: (close) => {
        root.commitActiveEdits()
        if (root.vm().dirty) {
            close.accepted = false
            root.runWhenClean(() => root.visible = false)
        }
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
        root.runRetrospective(() => root.doSave())
    }

    // Edits to a saved definition reinterpret past results, so they pass through the
    // retrospective warning; a never-saved draft applies the action immediately.
    function runRetrospective(action) {
        if (root.vm().draftFromLibrary) {
            root.pendingRetrospectiveAction = action
            retrospectivePrompt.open()
        } else {
            action()
        }
    }

    function requestMappingEdit(entryId, hash) {
        root.commitActiveEdits()
        root.runRetrospective(() => root.vm().setScenarioHash(entryId, hash))
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

    function filteredCatalogue() {
        const query = root.knownScenarioFilter.trim().toLowerCase()
        if (query === "") return []
        const catalogue = root.vm().scenarioCatalogue || []
        return catalogue.filter(entry => entry.name.toLowerCase().indexOf(query) !== -1)
    }

    function addSelectedKnownScenario() {
        const name = root.selectedKnownScenarioName
        const hash = root.selectedKnownScenarioHash
        if (hash === "") return
        const result = root.runTableCommand(() => root.vm().addKnownScenario(name, hash))
        if (!result || !result.ok) return
        root.knownScenarioFilter = ""
        root.selectedKnownScenarioHash = ""
        root.selectedKnownScenarioName = ""
    }

    // The submitted text is captured before runTableCommand commits the open edits, and the field
    // is cleared only once the session accepts it, so a rejected name stays for correction.
    function addFromField(field, add) {
        const name = field.text
        if (name.trim() === "") return
        const result = root.runTableCommand(() => add(name))
        if (result && result.ok) field.clear()
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
        const targets = [{id: "", label: qsTr("Uncategorized")}]
        const groups = root.vm().groups || []
        for (const group of groups) {
            if (group.kind === "category") {
                targets.push({id: group.id, label: group.name})
            } else {
                const parent = groups.find(g => g.id === group.parentId)
                targets.push({id: group.id, label: (parent ? parent.name : "") + " / " + group.name})
            }
        }
        return targets
    }

    // Position and size of the current row within the collection that holds it: a subcategory,
    // a category's direct scenarios (which precede its subcategories), or Uncategorized.
    function currentRowPlacement() {
        const info = root.currentRow
        const model = root.vm().tableModel
        if (!info || info.row === undefined || !model) return {position: -1, size: 0}
        if (info.subcategoryId !== "")
            return {position: info.row - info.subcategorySpanStart, size: info.subcategorySpanLength}
        let size = 0
        for (let r = info.categorySpanStart; r < info.categorySpanStart + info.categorySpanLength; ++r) {
            if (model.rowInfo(r).subcategoryId !== "") break
            ++size
        }
        return {position: info.row - info.categorySpanStart, size: size}
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
        if (name.trim() === "") return
        const groups = root.vm().groups || []
        const category = groups.find(g => g.id === categoryId)
        const hasSubcategories = groups.some(g => g.kind === "subcategory" && g.parentId === categoryId)
        if (category && !hasSubcategories && category.scenarioCount > 0) {
            subcategoryRelocationPrompt.categoryId = categoryId
            subcategoryRelocationPrompt.subcategoryName = name
            subcategoryRelocationPrompt.open()
            return
        }
        root.runTableCommand(() => root.vm().addSubcategory(categoryId, name))
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
        id: retrospectivePrompt
        objectName: "retrospectivePrompt"
        title: qsTr("Recalculate history?")
        text: qsTr("Saving these changes reinterprets past results under the edited definition.")
        informativeText: qsTr("Thresholds, membership, tier order, and hierarchy all feed the historical graphs.")
            + (root.normalizationIssues().length > 0
               ? "\n\n" + qsTr("These thresholds are not valid numbers and will be saved as missing: %1")
                     .arg(root.normalizationSummary())
               : "")
        buttons: MessageDialog.Save | MessageDialog.Cancel
        onButtonClicked: function (button) {
            const action = root.pendingRetrospectiveAction
            root.pendingRetrospectiveAction = null
            if (button === MessageDialog.Save) {
                if (action) action()
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

        function relocate(choice) {
            subcategoryRelocationPrompt.close()
            const categoryId = subcategoryRelocationPrompt.categoryId
            const name = subcategoryRelocationPrompt.subcategoryName
            root.runTableCommand(() => root.vm().addSubcategoryRelocating(categoryId, name, choice))
        }

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
        id: assignMenu
        objectName: "assignMenu"

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

    component ColorSwatchButton: Button {
        id: colorSwatchButton
        property string swatchKind
        property string swatchId
        property color swatchColor
        implicitWidth: 28
        implicitHeight: 28
        activeFocusOnTab: true
        background: Rectangle {
            color: colorSwatchButton.swatchColor
            radius: 4
            border.color: colorSwatchButton.activeFocus ? colorSwatchButton.palette.highlight : colorSwatchButton.palette.mid
            border.width: colorSwatchButton.activeFocus ? 2 : 1
        }
        onClicked: root.openColorDialog(colorSwatchButton.swatchKind, colorSwatchButton.swatchId,
                                        colorSwatchButton.swatchColor)
    }

    function openColorDialog(kind, id, color) {
        colorDialog.targetKind = kind
        colorDialog.targetId = id
        colorDialog.selectedColor = color
        colorDialog.open()
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ---- Library pane ------------------------------------------------
            ColumnLayout {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                spacing: 8

                Label {
                    text: qsTr("Library")
                    font.pixelSize: 18
                    font.bold: true
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Button {
                        objectName: "newBenchmarkButton"
                        text: qsTr("New")
                        onClicked: root.runWhenClean(() => root.vm().beginNewBenchmark())
                    }
                    Button {
                        objectName: "importPlaylistButton"
                        text: qsTr("Import playlist...")
                        onClicked: importDialog.open()
                    }
                    Button {
                        objectName: "openBenchmarkButton"
                        text: qsTr("Open")
                        enabled: root.selectedRow() !== null && root.selectedRow().openable
                        onClicked: {
                            const row = root.selectedRow()
                            root.runWhenClean(() => root.vm().openBenchmark(row.id))
                        }
                    }
                    Button {
                        objectName: "deleteBenchmarkButton"
                        text: qsTr("Delete")
                        enabled: root.selectedRow() !== null && root.selectedRow().deletable
                        onClicked: {
                            deleteConfirmPrompt.targetId = root.selectedRow().id
                            deleteConfirmPrompt.targetName = root.selectedRow().name
                            deleteConfirmPrompt.open()
                        }
                    }
                    Button {
                        objectName: "refreshLibraryButton"
                        text: qsTr("Refresh")
                        enabled: !root.vm().dirty
                        onClicked: root.vm().refresh()
                    }
                    Button {
                        objectName: "openDirectoryButton"
                        text: qsTr("Open directory")
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
                    delegate: ItemDelegate {
                        id: libraryEntry
                        required property var modelData
                        required property int index
                        width: libraryList.width
                        objectName: "libraryEntry_" + index
                        highlighted: root.selectedEntryFilename === libraryEntry.modelData.filename
                        onClicked: root.selectedEntryFilename = libraryEntry.modelData.filename
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

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: root.palette.mid
            }

            // ---- Editor pane ---------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6
                visible: root.vm().hasDraft

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: benchmarkNameField
                        objectName: "benchmarkNameField"
                        placeholderText: qsTr("Benchmark name")
                        Layout.fillWidth: true
                        selectByMouse: true
                        onEditingFinished: root.vm().setBenchmarkName(text)

                        // Typing into a TextField destroys a plain `text:` binding, and this pane is
                        // only hidden (never destroyed) when the draft closes, so the field would keep
                        // the old name across Discard/New. Binding re-asserts it whenever the field is
                        // not the one being edited.
                        Binding {
                            target: benchmarkNameField
                            property: "text"
                            value: root.vm().benchmarkName
                            when: !benchmarkNameField.activeFocus
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        objectName: "draftStatusBadge"
                        text: root.vm().draftTrackable ? qsTr("Trackable") : qsTr("Incomplete")
                    }
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.classificationColor(root.vm().draftTrackable
                            ? "Trackable" : "Incomplete")
                    }
                    Button {
                        objectName: "undoButton"
                        text: qsTr("Undo")
                        enabled: root.sessionRevision >= 0 && root.vm().canUndo === true
                        Accessible.name: qsTr("Undo the last table edit")
                        onClicked: {
                            root.commitActiveEdits()
                            root.tableStatus = ""
                            editorTable.undo()
                            root.sessionRevision++
                        }
                    }
                    Button {
                        objectName: "pasteButton"
                        text: qsTr("Paste")
                        Accessible.name: qsTr("Paste spreadsheet cells at the current cell or rank header")
                        onClicked: {
                            root.commitActiveEdits()
                            root.tableStatus = ""
                            editorTable.pasteFromClipboard()
                            root.sessionRevision++
                        }
                    }
                    Button {
                        objectName: "saveButton"
                        text: qsTr("Save")
                        onClicked: root.requestSave()
                    }
                    Button {
                        objectName: "discardButton"
                        text: qsTr("Discard")
                        onClicked: {
                            root.vm().discard()
                            root.statusMessage = ""
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

                // ---- Additions ------------------------------------------------------------------
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    TextField {
                        id: newScenarioField
                        objectName: "newScenarioField"
                        placeholderText: qsTr("Scenario name")
                        width: 160
                    }
                    Button {
                        objectName: "addScenarioButton"
                        text: qsTr("Add Scenario")
                        onClicked: root.addFromField(newScenarioField, name => root.vm().addUnplayedScenario(name))
                    }
                    // Profile-known scenarios are added by hash, so equal display names in the
                    // catalogue stay distinguishable; the free-text field above still adds an
                    // as-yet-unplayed scenario by name only.
                    TextField {
                        id: knownScenarioPicker
                        objectName: "knownScenarioPicker"
                        placeholderText: qsTr("Find a played scenario")
                        width: 170
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Find a played scenario to add by name")
                        onTextEdited: root.knownScenarioFilter = text
                        onEditingFinished: root.knownScenarioFilter = text
                    }
                    Button {
                        id: addKnownScenarioButton
                        objectName: "addKnownScenarioButton"
                        text: qsTr("Add Known Scenario")
                        activeFocusOnTab: true
                        Accessible.name: qsTr("Add the selected played scenario")
                        onClicked: root.addSelectedKnownScenario()
                    }
                    TextField {
                        id: newTierField
                        objectName: "newTierField"
                        placeholderText: qsTr("New tier name")
                        width: 130
                    }
                    Button {
                        objectName: "addTierButton"
                        text: qsTr("Add Tier")
                        onClicked: root.addFromField(newTierField, name => root.vm().addTier(name))
                    }
                    TextField {
                        id: newCategoryField
                        objectName: "newCategoryField"
                        placeholderText: qsTr("New category name")
                        width: 150
                    }
                    Button {
                        objectName: "addCategoryButton"
                        text: qsTr("Add Category")
                        onClicked: root.addFromField(newCategoryField, name => root.vm().addCategory(name))
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    visible: knownOptions.count > 0
                    Repeater {
                        id: knownOptions
                        model: root.filteredCatalogue()
                        delegate: ItemDelegate {
                            id: knownScenarioOption
                            required property var modelData
                            objectName: "knownScenarioOption_" + knownScenarioOption.modelData.hash
                            Layout.fillWidth: true
                            highlighted: root.selectedKnownScenarioHash === knownScenarioOption.modelData.hash
                            text: knownScenarioOption.modelData.name + "  ·  " + knownScenarioOption.modelData.hash
                            Accessible.name: qsTr("Select played scenario %1").arg(knownScenarioOption.text)
                            onClicked: {
                                root.selectedKnownScenarioHash = knownScenarioOption.modelData.hash
                                root.selectedKnownScenarioName = knownScenarioOption.modelData.name
                            }
                        }
                    }
                }

                // ---- Contextual controls --------------------------------------------------------
                Flow {
                    objectName: "rowActions"
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        text: qsTr("Rows:")
                        height: assignSelectionButton.height
                        verticalAlignment: Text.AlignVCenter
                    }
                    Button {
                        id: assignSelectionButton
                        objectName: "assignSelectionButton"
                        text: qsTr("Assign to group…")
                        Accessible.name: qsTr("Assign the selected scenarios to a group")
                        onClicked: assignMenu.popup(assignSelectionButton, 0, assignSelectionButton.height)
                    }
                    Button {
                        objectName: "moveRowUpButton"
                        text: qsTr("Move up")
                        enabled: root.currentRowPlacement().position > 0
                        Accessible.name: qsTr("Move the current scenario up within its group")
                        onClicked: root.moveCurrentRow(-1)
                    }
                    Button {
                        objectName: "moveRowDownButton"
                        readonly property var placement: root.currentRowPlacement()
                        text: qsTr("Move down")
                        enabled: placement.position >= 0 && placement.position < placement.size - 1
                        Accessible.name: qsTr("Move the current scenario down within its group")
                        onClicked: root.moveCurrentRow(1)
                    }
                    Button {
                        objectName: "removeScenarioButton"
                        text: qsTr("Remove")
                        Accessible.name: qsTr("Remove the selected scenarios")
                        onClicked: root.removeSelectedScenarios()
                    }
                }

                Flow {
                    objectName: "rankActions"
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.currentTier !== null
                    Label {
                        text: qsTr("Rank:")
                        height: rankSwatch.height
                        verticalAlignment: Text.AlignVCenter
                    }
                    ColorSwatchButton {
                        id: rankSwatch
                        objectName: "rankColorSwatch"
                        swatchKind: "tier"
                        swatchId: root.currentTierId
                        swatchColor: root.currentTier ? root.currentTier.color : "transparent"
                        Accessible.name: qsTr("Colour of rank %1").arg(root.currentTier && root.currentTier.name !== ""
                                                                        ? root.currentTier.name : qsTr("(unnamed)"))
                    }
                    BoundField {
                        id: rankNameField
                        objectName: "rankNameField"
                        width: 140
                        placeholderText: qsTr("Rank name")
                        boundText: root.currentTier ? root.currentTier.name : ""
                        Accessible.name: qsTr("Rank name")
                        onEditingFinished: if (root.currentTierId !== "") root.vm().renameTier(root.currentTierId, text)
                    }
                    Button {
                        objectName: "moveRankLeftButton"
                        text: qsTr("Move left")
                        enabled: root.currentTierIndex > 0
                        onClicked: root.moveCurrentRankBy(-1)
                    }
                    Button {
                        objectName: "moveRankRightButton"
                        text: qsTr("Move right")
                        enabled: root.currentTierIndex >= 0
                                 && root.currentTierIndex < (root.vm().tiers || []).length - 1
                        onClicked: root.moveCurrentRankBy(1)
                    }
                    Button {
                        objectName: "removeRankButton"
                        text: qsTr("Remove rank")
                        onClicked: root.removeCurrentRank()
                    }
                }

                Flow {
                    objectName: "groupActions"
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.currentGroup !== null
                    Label {
                        text: root.currentGroup && root.currentGroup.kind === "subcategory" ? qsTr("Subcategory:") : qsTr("Category:")
                        height: groupSwatch.height
                        verticalAlignment: Text.AlignVCenter
                    }
                    ColorSwatchButton {
                        id: groupSwatch
                        objectName: "groupColorSwatch"
                        swatchKind: "group"
                        swatchId: root.currentGroupId
                        swatchColor: root.currentGroup ? root.currentGroup.color : "transparent"
                        Accessible.name: qsTr("Colour of group %1").arg(root.currentGroup ? root.currentGroup.name : "")
                    }
                    BoundField {
                        id: groupNameField
                        objectName: "groupNameField"
                        width: 150
                        placeholderText: qsTr("Group name")
                        boundText: root.currentGroup ? root.currentGroup.name : ""
                        Accessible.name: qsTr("Group name")
                        onEditingFinished: if (root.currentGroupId !== "") root.vm().renameGroup(root.currentGroupId, text)
                    }
                    Button {
                        objectName: "moveGroupUpButton"
                        text: qsTr("Move up")
                        enabled: root.currentGroupIndex() > 0
                        onClicked: root.moveCurrentGroup(-1)
                    }
                    Button {
                        objectName: "moveGroupDownButton"
                        text: qsTr("Move down")
                        enabled: root.currentGroup !== null
                                 && root.currentGroupIndex() < root.groupSiblings(root.currentGroup).length - 1
                        onClicked: root.moveCurrentGroup(1)
                    }
                    Button {
                        objectName: "removeGroupButton"
                        text: qsTr("Remove group")
                        Accessible.name: qsTr("Remove the group; its scenarios return to Uncategorized")
                        onClicked: root.runTableCommand(() => root.vm().removeCategory(root.currentGroupId))
                    }
                    TextField {
                        id: newSubcategoryField
                        objectName: "newSubcategoryField"
                        visible: root.currentGroup !== null && root.currentGroup.kind === "category"
                        width: 150
                        placeholderText: qsTr("Subcategory name")
                    }
                    Button {
                        objectName: "addSubcategoryButton"
                        visible: newSubcategoryField.visible
                        text: qsTr("Add Subcategory")
                        onClicked: {
                            root.requestSubcategory(root.currentGroupId, newSubcategoryField.text)
                            newSubcategoryField.clear()
                        }
                    }
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

                // Mapping details for the current row; candidates for a resolved or mapped entry
                // stay disarmed until "Change mapping" is pressed.
                Pane {
                    id: mappingPanel
                    objectName: "mappingPanel"
                    Layout.fillWidth: true
                    visible: root.currentEntryId !== ""
                    padding: 4
                    property bool candidatesArmed: false
                    readonly property string mappingState: root.currentRow && root.currentRow.mappingState ? root.currentRow.mappingState : "unresolved"
                    readonly property var candidates: root.currentRow && root.currentRow.mappingCandidates ? root.currentRow.mappingCandidates : []
                    readonly property bool armed: mappingPanel.candidatesArmed || mappingPanel.mappingState === "ambiguous"
                    Connections {
                        target: root
                        function onCurrentEntryIdChanged() { mappingPanel.candidatesArmed = false }
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 2
                        RowLayout {
                            spacing: 8
                            Label {
                                text: qsTr("Mapping for %1:").arg(root.currentRow && root.currentRow.name ? root.currentRow.name : qsTr("(unnamed)"))
                            }
                            Label {
                                objectName: "matchLabel"
                                text: root.matchStateLabel(mappingPanel.mappingState)
                                color: root.palette.placeholderText
                            }
                            Button {
                                objectName: "changeMappingButton"
                                text: qsTr("Change mapping")
                                visible: root.offersMappingEdit(mappingPanel.mappingState)
                                Accessible.name: qsTr("Change the scenario mapping")
                                onClicked: mappingPanel.candidatesArmed = !mappingPanel.candidatesArmed
                            }
                            Button {
                                objectName: "clearMappingButton"
                                text: qsTr("Clear mapping")
                                visible: root.offersMappingEdit(mappingPanel.mappingState)
                                Accessible.name: qsTr("Clear the scenario mapping")
                                onClicked: root.requestMappingEdit(root.currentEntryId, "")
                            }
                        }
                        Repeater {
                            model: mappingPanel.candidates
                            delegate: ItemDelegate {
                                id: candidateOption
                                required property var modelData
                                objectName: "candidateOption_" + candidateOption.modelData.hash
                                Layout.fillWidth: true
                                enabled: mappingPanel.armed
                                opacity: mappingPanel.armed ? 1 : 0.4
                                text: root.candidateText(candidateOption.modelData)
                                Accessible.name: qsTr("Map to %1").arg(candidateOption.text)
                                onClicked: root.requestMappingEdit(root.currentEntryId, candidateOption.modelData.hash)
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
                }
            }

            Label {
                visible: !root.vm().hasDraft
                text: qsTr("Create, import, or open a benchmark to edit it.")
                color: root.palette.placeholderText
            }
        }
    }
}
