import QtQuick
import QtTest
import QtQuick.Dialogs
import KsvTestSupport
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "BenchmarkManagerDialogTest"
    when: windowShown
    width: 1040
    height: 700
    visible: true

    Component {
        id: dialogComponent
        BenchmarkManagerDialog {}
    }

    Component {
        id: fixtureComponent
        BenchmarkTableModelFixture {}
    }

    function makeEntries() {
        return [
            {id: "bench-A", name: "Alpha", filename: "a.json", classification: "Incomplete",
             openable: true, deletable: true},
            {id: "", name: "bad.json", filename: "bad.json", classification: "Invalid",
             openable: false, deletable: false}
        ]
    }

    // Assigning a plain JS object to a `property var` stores a QML-owned copy. The dialog routes
    // every command through its table's copy (BenchmarkManagerDialog.vm()), so recorder state is
    // read back through `dialog.editorTable.manager`, never through the object passed in.
    function openDialog(props) {
        const dialog = createTemporaryObject(dialogComponent, testCase, props)
        verify(dialog !== null, "BenchmarkManagerDialog failed to instantiate")
        dialog.open()
        verify(waitForRendering(dialog.contentItem), "BenchmarkManagerDialog content never became visible")
        return dialog
    }

    function openWithFake(overrides) {
        return openDialog({benchmarkManagerVm: TestDoubles.makeFakeBenchmarkManagerVm(overrides || {})})
    }

    function vm(dialog) {
        return dialog.editorTable.manager
    }

    // `fixtureRef`, when given, receives the fixture so a command double can publish a new
    // projection through it and genuinely reset the model.
    function openWithTable(desc, overrides, fixtureRef) {
        const fixture = createTemporaryObject(fixtureComponent, testCase)
        if (fixtureRef) fixtureRef.fixture = fixture
        fixture.setProjection(TestDoubles.benchmarkTableProjection(desc))
        return openWithFake(Object.assign({hasDraft: true, tableModel: fixture.model}, overrides || {}))
    }

    function tableCell(dialog, entryId, column) {
        return ItemLookup.findByObjectName(dialog.contentItem, "cell_" + entryId + "_" + column)
    }

    function find(dialog, objectName) {
        return ItemLookup.findByObjectName(dialog.contentItem, objectName)
    }

    function test_importFailureShowsErrorAndCreatesNoDraft() {
        const dialog = openWithFake({importResult: {ok: false, error: "not a playlist"}})

        dialog.importFromUrl("file:///tmp/playlist.json")

        tryVerify(() => vm(dialog).importCalls.length === 1)
        const label = find(dialog, "importStatusLabel")
        verify(label !== null, "no importStatusLabel found")
        tryCompare(label, "visible", true)
        compare(label.text, "not a playlist")
    }

    function test_importSuccessReportsSkippedDuplicates() {
        const dialog = openWithFake({importResult: {ok: true, skipped: [2, 5]}})

        dialog.importFromUrl("file:///tmp/playlist.json")

        const label = find(dialog, "importStatusLabel")
        verify(label.text.indexOf("2") !== -1, "import status should report the duplicate count")
    }

    function test_closingWithNoDirtyDraftClosesWithoutPrompt() {
        const dialog = openWithFake({dirty: false})

        dialog.close()

        tryCompare(dialog, "visible", false)
        compare(vm(dialog).discardCalls, 0)
    }

    function test_dirtyCloseRaisesSaveDiscardCancelPrompt() {
        const dialog = openWithFake({dirty: true, hasDraft: true})

        dialog.close()

        tryCompare(dialog, "visible", true)
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        compare(vm(dialog).discardCalls, 0)
    }

    function test_discardingOnCloseCallsDiscardThenCloses() {
        const dialog = openWithFake({dirty: true, hasDraft: true})

        dialog.close()
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)

        tryCompare(vm(dialog), "discardCalls", 1)
        tryCompare(dialog, "visible", false)
    }

    function test_dirtyNewTransitionShowsPromptAndDiscardRunsThePendingAction() {
        const dialog = openWithFake({dirty: true, hasDraft: true})

        mouseClick(find(dialog, "newBenchmarkButton"))
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        compare(vm(dialog).beginNewCalls, 0, "beginNewBenchmark must wait for the prompt decision")

        dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)

        tryCompare(vm(dialog), "discardCalls", 1)
        tryCompare(vm(dialog), "beginNewCalls", 1)
    }

    function test_dirtyTransitionCancelLeavesTheDraftUntouched() {
        const dialog = openWithFake({dirty: true, hasDraft: true})

        mouseClick(find(dialog, "newBenchmarkButton"))
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)

        compare(vm(dialog).beginNewCalls, 0)
        compare(vm(dialog).discardCalls, 0)
        compare(vm(dialog).saveCalls, 0)
    }

    function test_dirtyTransitionFailedSaveKeepsThePendingActionPending() {
        const dialog = openWithFake({
            dirty: true, hasDraft: true,
            saveResult: {ok: false, error: "Give the benchmark a name before saving."}
        })

        mouseClick(find(dialog, "newBenchmarkButton"))
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(vm(dialog), "saveCalls", 1)
        compare(vm(dialog).beginNewCalls, 0, "a failed save must not run the pending transition")
        compare(dialog.visible, true)
    }

    function test_saveButtonDelegatesToVm() {
        const dialog = openWithFake({hasDraft: true, dirty: true})

        mouseClick(find(dialog, "saveButton"))

        tryCompare(vm(dialog), "saveCalls", 1)
        const status = find(dialog, "saveStatusLabel")
        tryCompare(status, "visible", true)
    }

    function test_retrospectiveWarningShownBeforeSavingAnOpenedBenchmark() {
        const dialog = openWithFake({hasDraft: true, dirty: true, draftFromLibrary: true})

        mouseClick(find(dialog, "saveButton"))

        tryCompare(dialog.retrospectivePrompt, "visible", true)
        compare(vm(dialog).saveCalls, 0, "save must wait for the retrospective warning confirmation")

        dialog.retrospectivePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(vm(dialog), "saveCalls", 1)
    }

    function test_staleBaselineWarning() {
        const dialog = openWithFake({hasDraft: true, dirty: true, draftFromLibrary: true,
                                     baselineStale: true})

        const warning = find(dialog, "staleBaselineWarning")
        verify(warning !== null, "the editor must surface a stale-baseline warning")
        compare(warning.visible, true)

        // The editor is not hidden and Discard stays available.
        compare(find(dialog, "benchmarkNameField").visible, true)
        compare(find(dialog, "discardButton").enabled, true)
    }

    function test_deleteRequiresConfirmation() {
        const dialog = openWithFake({libraryEntries: makeEntries()})

        mouseClick(find(dialog, "libraryEntry_0"))
        mouseClick(find(dialog, "deleteBenchmarkButton"))

        tryCompare(dialog.deleteConfirmPrompt, "visible", true)
        compare(vm(dialog).deleteCalls.length, 0, "delete must wait for confirmation")

        dialog.deleteConfirmPrompt.buttonClicked(MessageDialog.Yes, MessageDialog.YesRole)

        tryVerify(() => vm(dialog).deleteCalls.length === 1)
        compare(vm(dialog).deleteCalls, ["bench-A"])
    }

    function test_problemEntriesCannotBeOpenedOrDeleted() {
        const dialog = openWithFake({libraryEntries: makeEntries()})

        mouseClick(find(dialog, "libraryEntry_1"))

        compare(find(dialog, "deleteBenchmarkButton").enabled, false)
        compare(find(dialog, "openBenchmarkButton").enabled, false)
        compare(vm(dialog).deleteCalls.length, 0)
    }

    function test_refreshButtonDelegatesToVmAndIsDisabledWhileDirty() {
        const dirtyDialog = openWithFake({dirty: true})
        compare(find(dirtyDialog, "refreshLibraryButton").enabled, false,
                "refresh must be unavailable while the manager contains unsaved edits")

        const cleanDialog = openWithFake({dirty: false})
        mouseClick(find(cleanDialog, "refreshLibraryButton"))
        tryCompare(vm(cleanDialog), "refreshCalls", 1)
    }

    function test_clickingValidationIssueHighlightsItsElement() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            validationIssues: [{message: "A scenario is missing a threshold for some tier.",
                                targetId: "s-unr", tierId: "t1"}]
        })
        compare(vm(dialog).currentCell.entryId, undefined, "nothing is focused before the issue is clicked")

        mouseClick(find(dialog, "validationIssue_0"))

        tryVerify(() => vm(dialog).currentCell.entryId === "s-unr")
        compare(vm(dialog).currentCell.tierId, "t1", "the issue focuses its exact threshold cell")
        compare(tableCell(dialog, "s-unr", "t1").stateLabel, qsTr("Missing threshold"))
    }

    function test_reopenedBenchmarkShowsScenariosTiersAndThresholds() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            dirty: true, draftFromLibrary: true,
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}]
        })

        tryVerify(() => tableCell(dialog, "s-res", "scenario") !== null)
        compare(tableCell(dialog, "s-res", "scenario").cellText, "resolved one")
        compare(tableCell(dialog, "s-res", "t1").cellText, "90", "the saved threshold renders against its tier")
        const header = ItemLookup.findByObjectName(dialog.contentItem, "rankHeader_t1")
        verify(header !== null, "the tier ladder renders as the shared header")
        compare(header.headerName, "Gold")
    }

    function test_thresholdEditingDelegatesToVm() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc())

        verify(dialog.editorTable.focusCell({entryId: "s-res", tierId: "t1"}))
        keyClick(Qt.Key_Return)
        tryVerify(() => dialog.editorTable.activeEditor !== null)
        dialog.editorTable.activeEditor.selectAll()
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_5)
        keyClick(Qt.Key_0)
        keyClick(Qt.Key_Return)

        tryVerify(() => vm(dialog).editThresholdTextCalls.length === 1)
        compare(vm(dialog).editThresholdTextCalls[0], ["s-res", "t1", "250"],
                "the typed text goes to the view model unconverted")
        compare(vm(dialog).setThresholdCalls.length, 0, "no JavaScript numeric conversion")
    }

    function test_addScenarioAndTierDelegateToVm() {
        const dialog = openWithFake({hasDraft: true})

        const scenarioField = find(dialog, "newScenarioField")
        scenarioField.text = "1w6ts"
        mouseClick(find(dialog, "addScenarioButton"))
        const tierField = find(dialog, "newTierField")
        tierField.text = "Gold"
        mouseClick(find(dialog, "addTierButton"))

        tryVerify(() => vm(dialog).addUnplayedScenarioCalls.length === 1)
        tryVerify(() => vm(dialog).addTierCalls.length === 1)
        compare(vm(dialog).addUnplayedScenarioCalls, ["1w6ts"])
        compare(vm(dialog).addTierCalls, ["Gold"])
        compare(scenarioField.text, "", "the scenario field should clear after adding")
        compare(tierField.text, "", "the tier field should clear after adding")
    }

    // ---- M2: known-scenario and mapping-resolution controls ---------------

    function test_bothFreeTextAndKnownScenarioAddPathsAreOffered() {
        const dialog = openWithFake({
            hasDraft: true,
            scenarioCatalogue: TestDoubles.benchmarkManagerScenarioCatalogue()
        })

        verify(find(dialog, "newScenarioField") !== null,
               "the free-text Add unplayed scenario path must remain")
        verify(find(dialog, "addScenarioButton") !== null,
               "the free-text add button must remain")
        verify(find(dialog, "knownScenarioPicker") !== null,
               "a catalogue-backed known-scenario picker must exist beside the free-text field")
        verify(find(dialog, "addKnownScenarioButton") !== null,
               "the known-scenario add button must exist")
    }

    function test_knownScenarioPickerFiltersCatalogueAndAddsByHash() {
        const dialog = openWithFake({
            hasDraft: true,
            scenarioCatalogue: TestDoubles.benchmarkManagerScenarioCatalogue()
        })

        const picker = find(dialog, "knownScenarioPicker")
        verify(picker !== null, "no knownScenarioPicker found")
        picker.forceActiveFocus()
        picker.text = "1w4ts"
        picker.editingFinished()

        // Both "1w4ts" rows survive the filter; each is addressable by its own hash.
        const optionA = find(dialog, "knownScenarioOption_hash-a")
        const optionB = find(dialog, "knownScenarioOption_hash-b")
        verify(optionA !== null && optionB !== null,
               "duplicate names must be offered as distinct hash-keyed options")
        verify(find(dialog, "knownScenarioOption_hash-c") === null,
               "non-matching catalogue rows must be filtered out")

        mouseClick(optionB)
        mouseClick(find(dialog, "addKnownScenarioButton"))

        tryVerify(() => vm(dialog).addKnownScenarioCalls.length === 1)
        compare(vm(dialog).addKnownScenarioCalls[0], ["1w4ts", "hash-b"])
    }

    function test_everyMatchingStateRendersItsTextualLabel() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}]
        })

        const expected = {"s-res": "Resolved", "s-map": "Mapped, unavailable", "s-unr": "Unresolved",
                          "s-amb": "Ambiguous", "s-auto": "Pending automatic mapping"}
        for (const entryId in expected) {
            verify(dialog.editorTable.focusCell({entryId: entryId}))
            tryCompare(find(dialog, "matchLabel"), "text", expected[entryId])
        }
    }

    function test_ambiguousCandidateMetadataShownAndSelectionSetsHash() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc())

        verify(dialog.editorTable.focusCell({entryId: "s-amb"}))
        tryVerify(() => find(dialog, "candidateOption_hash-b") !== null)
        const candidateB = find(dialog, "candidateOption_hash-b")
        verify(candidateB.text.indexOf("hash-b") !== -1, "the candidate hash must be recognizable")
        verify(candidateB.text.indexOf("7") !== -1, "the candidate run count must be shown")
        verify(candidateB.text.indexOf("2024") !== -1, "the candidate last-played date must be shown")

        mouseClick(candidateB)

        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 1)
        compare(vm(dialog).setScenarioHashCalls[0], ["s-amb", "hash-b"])
    }

    function test_changeMappingRoutesThroughSetScenarioHash() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc())

        verify(dialog.editorTable.focusCell({entryId: "s-res"}))
        const changeButton = find(dialog, "changeMappingButton")
        tryCompare(changeButton, "visible", true)
        const candidate = find(dialog, "candidateOption_hash-a")
        verify(candidate !== null, "Change mapping must expose candidate hashes to pick from")
        compare(candidate.enabled, false, "resolved candidates stay disarmed until Change mapping")
        waitForRendering(dialog.contentItem)
        mouseClick(changeButton)
        tryCompare(candidate, "enabled", true)
        mouseClick(candidate)

        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 1)
        compare(vm(dialog).setScenarioHashCalls[0][0], "s-res")
        compare(vm(dialog).setScenarioHashCalls[0][1], "hash-a")
    }

    function test_clearMappingPassesNoHashAndKeepsEntryThresholdsAndPlacement() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}]
        })

        verify(dialog.editorTable.focusCell({entryId: "s-map"}))
        const clearButton = find(dialog, "clearMappingButton")
        tryCompare(clearButton, "visible", true)
        waitForRendering(dialog.contentItem)
        mouseClick(clearButton)

        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 1)
        compare(vm(dialog).setScenarioHashCalls[0][0], "s-map")
        compare(vm(dialog).setScenarioHashCalls[0][1], "", "Clear mapping passes no hash")

        // The row, its thresholds and its hierarchy placement are untouched by a clear.
        verify(tableCell(dialog, "s-map", "scenario") !== null, "the scenario entry must remain")
        compare(tableCell(dialog, "s-map", "t1").cellText, "80", "thresholds must be retained")
        compare(vm(dialog).clearThresholdCalls.length, 0)
        compare(vm(dialog).moveScenarioCalls.length + vm(dialog).assignScenariosCalls.length, 0)
    }

    function test_retrospectiveWarningPrecedesAMappingEditOnASavedDraft() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {dirty: true, draftFromLibrary: true})

        verify(dialog.editorTable.focusCell({entryId: "s-amb"}))
        tryVerify(() => find(dialog, "candidateOption_hash-b") !== null)
        mouseClick(find(dialog, "candidateOption_hash-b"))

        tryCompare(dialog.retrospectivePrompt, "visible", true)
        compare(vm(dialog).setScenarioHashCalls.length, 0,
                "a mapping edit on a saved definition must wait for the retrospective warning")

        dialog.retrospectivePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 1)
        compare(vm(dialog).setScenarioHashCalls[0], ["s-amb", "hash-b"])
    }

    function test_automaticResolutionFailureBannerIsNonModalAndClearsOnNextPublication() {
        const failing = openWithFake({
            hasDraft: true,
            resolutionWriteFailed: true
        })

        const banner = find(failing, "resolutionFailureBanner")
        verify(banner !== null, "an automatic-resolution write failure must surface a banner")
        tryCompare(banner, "visible", true)
        // Non-modal: the rest of the editor stays usable while the banner is shown.
        compare(find(failing, "addScenarioButton").enabled, true,
                "the failure banner must not block the editor")

        const recovered = openWithFake({
            hasDraft: true,
            resolutionWriteFailed: false
        })
        const clearedBanner = find(recovered, "resolutionFailureBanner")
        verify(clearedBanner === null || clearedBanner.visible === false,
               "the banner must clear once a later reconciliation publishes without failure")
    }

    function test_knownScenarioControlsAreKeyboardAndAccessibilityOrdered() {
        const dialog = openWithFake({
            hasDraft: true,
            scenarioCatalogue: TestDoubles.benchmarkManagerScenarioCatalogue()
        })

        const picker = find(dialog, "knownScenarioPicker")
        const addButton = find(dialog, "addKnownScenarioButton")
        verify(picker !== null && addButton !== null, "known-scenario controls must exist")

        compare(picker.activeFocusOnTab, true, "the picker must be reachable by keyboard")
        compare(addButton.activeFocusOnTab, true, "the add button must be reachable by keyboard")
        verify(String(picker.Accessible.name) !== "", "the picker must carry an accessible name")
        verify(String(addButton.Accessible.name) !== "", "the add button must carry an accessible name")

        picker.forceActiveFocus()
        compare(picker.nextItemInFocusChain(), addButton,
                "tab order must run from the picker to its add button")
    }

    // ---- Committing active input before gates and commands -------------------------------------

    // A double whose threshold edits behave like the session: retained input dirties the draft and
    // is listed as a Save consequence.
    function recordingInputOverrides() {
        return {
            editThresholdText: function (entryId, tierId, text) {
                this.commandLog.push("editThresholdText")
                this.editThresholdTextCalls.push([entryId, tierId, text])
                this.dirty = true
                this.normalizationIssues = [{entryId: entryId, tierId: tierId, text: text,
                                             message: "Not a number. Saving stores this threshold as missing."}]
                return {ok: true}
            },
            save: function () {
                this.commandLog.push("save")
                this.saveCalls++
                return this.saveResult || {ok: true}
            },
            setBenchmarkName: function (name) {
                this.commandLog.push("setBenchmarkName")
                this.setBenchmarkNameCalls.push(name)
            },
            beginNewBenchmark: function () { this.commandLog.push("beginNewBenchmark"); this.beginNewCalls++ }
        }
    }

    function typeIntoCell(dialog, entryId, tierId, text) {
        verify(dialog.editorTable.focusCell({entryId: entryId, tierId: tierId}))
        keyClick(Qt.Key_Return)
        tryVerify(() => dialog.editorTable.activeEditor !== null)
        dialog.editorTable.activeEditor.selectAll()
        for (const ch of text) keyClick(ch)
        compare(vm(dialog).editThresholdTextCalls.length, 0, "the text is still only in the editor")
    }

    function test_activeInputPrecedesSaveAndTransitions() {
        {
            const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(),
                Object.assign({dirty: false, draftFromLibrary: true}, recordingInputOverrides()))
            typeIntoCell(dialog, "s-res", "t1", "12oops")

            mouseClick(find(dialog, "saveButton"))

            compare(vm(dialog).editThresholdTextCalls, [["s-res", "t1", "12oops"]], "Save commits the editor first")
            tryCompare(dialog.retrospectivePrompt, "visible", true)
            verify(String(dialog.retrospectivePrompt.informativeText).indexOf("12oops") !== -1,
                   "the existing gate names what Save will store as missing")
            const warning = find(dialog, "normalizationWarning")
            verify(warning !== null && warning.visible, "the consequence is listed in the editor")
            verify(warning.text.indexOf("12oops") !== -1)

            dialog.retrospectivePrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)
            compare(vm(dialog).saveCalls, 0)
            compare(vm(dialog).discardCalls, 0, "cancel keeps the retained input")
            compare(vm(dialog).undoCalls, 0)
            verify(find(dialog, "normalizationWarning").visible)
        }
        {
            // A draft-replacing transition checks dirty only after the editor is committed.
            const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(),
                Object.assign({dirty: false}, recordingInputOverrides()))
            typeIntoCell(dialog, "s-res", "t1", "oops")

            mouseClick(find(dialog, "newBenchmarkButton"))

            compare(vm(dialog).commandLog[0], "editThresholdText")
            tryCompare(dialog.dirtyClosePrompt, "visible", true)
            compare(vm(dialog).beginNewCalls, 0, "the now-dirty draft is not replaced silently")
            dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)
            compare(vm(dialog).beginNewCalls, 0)
            compare(vm(dialog).discardCalls, 0)
            compare(dialog.pendingAction, null, "no deferred transition remains armed")
        }
        {
            // The benchmark name field is committed before Save as well.
            const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(),
                Object.assign({dirty: true, benchmarkName: "Old"}, recordingInputOverrides()))
            const nameField = find(dialog, "benchmarkNameField")
            nameField.forceActiveFocus()
            nameField.text = "New name"

            dialog.requestSave()

            compare(vm(dialog).commandLog, ["setBenchmarkName", "save"])
            compare(vm(dialog).setBenchmarkNameCalls, ["New name"])
        }
    }

    // An addition double that, like the session, publishes a reshaped projection: the model reset
    // destroys an editor still open, whose deferred commit would then land after the addition.
    function resettingAddition(name, ref, result) {
        return function () {
            this.commandLog.push(name)
            this[name + "Calls"].push(Array.prototype.slice.call(arguments))
            const desc = TestDoubles.benchmarkResolutionDesc()
            desc.tiers.push({id: "t2", name: "Platinum"})
            desc.rows.push({entryId: "added", name: "added", cells: {}})
            ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(desc))
            return result || {ok: true, createdId: "added"}
        }
    }

    function clickWhileEditing(dialog, buttonName) {
        const button = find(dialog, buttonName)
        verify(button !== null, "no " + buttonName)
        button.clicked()
    }

    function test_activeInputPrecedesStructuralCommands() {
        const additions = [
            {name: "addUnplayedScenario", prepare: (dialog) => find(dialog, "newScenarioField").text = "Fresh",
             run: (dialog) => clickWhileEditing(dialog, "addScenarioButton")},
            {name: "addKnownScenario", prepare: (dialog) => {
                dialog.selectedKnownScenarioName = "1w4ts"
                dialog.selectedKnownScenarioHash = "hash-b"
            }, run: (dialog) => clickWhileEditing(dialog, "addKnownScenarioButton")},
            {name: "addTier", prepare: (dialog) => find(dialog, "newTierField").text = "Silver",
             run: (dialog) => clickWhileEditing(dialog, "addTierButton")},
            {name: "addCategory", prepare: (dialog) => find(dialog, "newCategoryField").text = "Flicking",
             run: (dialog) => clickWhileEditing(dialog, "addCategoryButton")}
        ]
        const commands = [
            {name: "assignScenarios", run: (dialog) => {
                vm(dialog).selectedEntryIds = ["s-res"]
                dialog.assignSelection("")
            }},
            {name: "reorderScenario", run: (dialog) => dialog.moveCurrentRow(1)},
            {name: "reorderTier", run: (dialog) => {
                verify(dialog.editorTable.focusCell({tierId: "t2"}))
                tryCompare(dialog, "currentTierId", "t2")
                dialog.moveCurrentRankBy(-1)
            }},
            {name: "removeScenarios", run: (dialog) => dialog.removeSelectedScenarios()},
            {name: "removeTier", run: (dialog) => dialog.removeCurrentRank()}
        ].concat(additions)
        for (const command of commands) {
            const desc = TestDoubles.benchmarkResolutionDesc()
            desc.tiers.push({id: "t2", name: "Platinum"})
            const ref = {}
            const overrides = {canUndo: true, tiers: desc.tiers}
            if (command.prepare) overrides[command.name] = resettingAddition(command.name, ref)
            const dialog = openWithTable(desc, Object.assign(overrides, recordingInputOverrides()), ref)
            if (command.prepare) command.prepare(dialog)
            typeIntoCell(dialog, "s-res", "t1", "oops")
            // The structural command runs while the editor is still open.
            verify(dialog.editorTable.activeEditor !== null)
            const log = vm(dialog).commandLog
            const before = log.length

            command.run(dialog)
            wait(50)

            compare(log[before], "editThresholdText", command.name + ": the text is committed first")
            compare(log.filter(c => c === "editThresholdText").length, 1,
                    command.name + ": the editor's text is committed exactly once: " + JSON.stringify(log))
            verify(log.indexOf(command.name) > log.indexOf("editThresholdText"),
                   command.name + " must follow the commit: " + JSON.stringify(log))
            compare(vm(dialog).editThresholdTextCalls[0], ["s-res", "t1", "oops"],
                    "the committed text is addressed by IDs, never by a row or column number")

            // Two Undo steps: the structural command, then the committed text.
            mouseClick(find(dialog, "undoButton"))
            mouseClick(find(dialog, "undoButton"))
            tryCompare(vm(dialog), "undoCalls", 2)
            compare(log.slice(-2), ["undo", "undo"])
        }
    }

    function test_rejectedAdditionKeepsItsInputAndExplains() {
        const ref = {}
        const error = "That tier no longer exists."
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), Object.assign({
            addTier: resettingAddition("addTier", ref, {ok: false, error: error})
        }, recordingInputOverrides()), ref)
        const field = find(dialog, "newTierField")
        field.text = "Silver"
        typeIntoCell(dialog, "s-res", "t1", "oops")

        clickWhileEditing(dialog, "addTierButton")

        compare(vm(dialog).commandLog.slice(0, 2), ["editThresholdText", "addTier"])
        tryCompare(find(dialog, "tableStatusLabel"), "text", error)
        compare(field.text, "Silver", "a rejected addition leaves its input for correction")
    }

    // An earlier command's rejection must not hide why a later Paste did nothing.
    function test_pasteExplanationReplacesEarlierRejection() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            addTier: function (name) { this.commandLog.push("addTier"); return {ok: false, error: "That tier no longer exists."} }
        })
        find(dialog, "newTierField").text = "Silver"
        mouseClick(find(dialog, "addTierButton"))
        tryCompare(find(dialog, "tableStatusLabel"), "text", "That tier no longer exists.")

        mouseClick(find(dialog, "pasteButton"))

        tryCompare(find(dialog, "tableStatusLabel"), "text",
                   "Select a scenario cell, threshold cell or rank header before pasting.")
    }

    // Only an empty table appends; a populated one with nothing focused must not silently grow.
    function test_pasteWithoutDestinationExplainsAndReadsNoClipboard() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), {
            clipboardReads: 0,
            clipboardText: function () { this.clipboardReads++; return "New\t1" }
        })

        mouseClick(find(dialog, "pasteButton"))

        const message = "Select a scenario cell, threshold cell or rank header before pasting."
        compare(dialog.editorTable.statusText, message)
        tryCompare(find(dialog, "tableStatusLabel"), "text", message)
        compare(vm(dialog).clipboardReads, 0, "the clipboard is not read without a destination")
        compare(vm(dialog).pasteTextCalls.length, 0)
        compare(vm(dialog).commandLog, [])
    }

    function test_failedSaveRetainsInputAndAbandonsDeferredAction() {
        const dialog = openWithTable(TestDoubles.benchmarkResolutionDesc(), Object.assign({
            dirty: true, hasDraft: true,
            saveResult: {ok: false, error: "Saving failed. The stored benchmark is unchanged."},
            normalizationIssues: [{entryId: "s-res", tierId: "t1", text: "oops", message: "Not a number."}]
        }, recordingInputOverrides()))

        mouseClick(find(dialog, "newBenchmarkButton"))
        tryCompare(dialog.dirtyClosePrompt, "visible", true)
        dialog.dirtyClosePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(vm(dialog), "saveCalls", 1)
        compare(vm(dialog).beginNewCalls, 0, "a failed save does not run the transition")
        compare(vm(dialog).discardCalls, 0, "a failed save keeps the session and its input")
        verify(find(dialog, "normalizationWarning").visible)
        compare(find(dialog, "saveStatusLabel").text, "Saving failed. The stored benchmark is unchanged.")

        // A later, unrelated successful Save must not run the abandoned transition.
        vm(dialog).saveResult = {ok: true}
        mouseClick(find(dialog, "saveButton"))
        tryCompare(vm(dialog), "saveCalls", 2)
        compare(vm(dialog).beginNewCalls, 0)
    }
}
