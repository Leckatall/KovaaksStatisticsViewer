import QtQuick
import QtTest
import QtQuick.Dialogs
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

    function makeFakeTree() {
        return {
            nodeId: "", kind: "uncategorized", name: "Uncategorized",
            children: [
                { nodeId: "s1", kind: "scenario", name: "1w6ts", hasHash: false,
                  thresholds: [{tierId: "t1", tierName: "Gold", score: 100, hasValue: true}] },
                { nodeId: "c1", kind: "category", name: "Clicking", color: "#009600",
                  children: [
                      { nodeId: "sub1", kind: "subcategory", name: "Static", color: "#009600",
                        children: [{ nodeId: "s2", kind: "scenario", name: "voxTarget",
                                     hasHash: true, thresholds: [] }] }
                  ] }
            ]
        }
    }

    function makeEntries() {
        return [
            {id: "bench-A", name: "Alpha", filename: "a.json", classification: "Incomplete",
             openable: true, deletable: true},
            {id: "", name: "bad.json", filename: "bad.json", classification: "Invalid",
             openable: false, deletable: false}
        ]
    }

    // Assigning a plain JS object to the dialog's `property var` stores a QML-owned copy;
    // the dialog's handlers mutate that copy, so recorder state is read back through
    // `dialog.benchmarkManagerVm`, never through the local object passed to openDialog().
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
        return dialog.benchmarkManagerVm
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
        const dialog = openWithFake({
            hasDraft: true,
            root: makeFakeTree(),
            validationIssues: [{message: "A scenario is missing a threshold for some tier.", targetId: "s1"}]
        })

        const node = find(dialog, "treeNode_s1")
        verify(node !== null, "no tree node found for scenario s1")
        compare(node.color.a, 0, "the target should not be highlighted before the issue is clicked")

        mouseClick(find(dialog, "validationIssue_0"))

        verify(node.color.a > 0, "clicking a validation issue should highlight the targeted element")
    }

    function test_reopenedBenchmarkShowsScenariosTiersAndThresholds() {
        const dialog = openWithFake({
            hasDraft: true, dirty: true, draftFromLibrary: true,
            root: makeFakeTree(),
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}]
        })

        const scenarioNode = find(dialog, "treeNode_s1")
        verify(scenarioNode !== null, "reopened scenarios should render in the tree")
        const thresholdField = find(dialog, "thresholdField_s1_t1")
        verify(thresholdField !== null, "the saved threshold should render against its tier")
        compare(thresholdField.text, "100")
        const tierNameField = find(dialog, "tierName_t1")
        verify(tierNameField !== null, "the tier ladder should render")
        compare(tierNameField.text, "Gold")
    }

    function test_thresholdEditingDelegatesToVm() {
        const dialog = openWithFake({
            hasDraft: true, root: makeFakeTree(),
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}]
        })

        const field = find(dialog, "thresholdField_s1_t1")
        field.forceActiveFocus()
        field.text = "250"
        field.editingFinished()

        tryVerify(() => vm(dialog).setThresholdCalls.length === 1)
        compare(vm(dialog).setThresholdCalls[0][0], "s1")
        compare(vm(dialog).setThresholdCalls[0][1], "t1")
        compare(vm(dialog).setThresholdCalls[0][2], 250)
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
}
