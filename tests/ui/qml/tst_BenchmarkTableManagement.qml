import QtQuick
import QtTest
import QtQuick.Dialogs
import KsvTestSupport
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "BenchmarkTableManagementTest"
    when: windowShown
    width: 1200
    height: 800
    visible: true

    Component {
        id: fixtureComponent
        BenchmarkTableModelFixture {}
    }

    Component {
        id: dialogComponent
        BenchmarkManagerDialog {}
    }

    function openTableDialog(overrides) {
        const fixture = createTemporaryObject(fixtureComponent, testCase)
        fixture.setProjection(TestDoubles.benchmarkTableProjection(TestDoubles.benchmarkManagementDesc()))
        const vm = TestDoubles.makeFakeBenchmarkManagerVm(Object.assign({
            hasDraft: true, tableModel: fixture.model, groups: TestDoubles.benchmarkManagementGroups(),
            tiers: [{id: "t1", name: "Gold", color: "#FFD700"}, {id: "t2", name: "", color: "#808080"}]
        }, overrides || {}))
        const dialog = createTemporaryObject(dialogComponent, testCase, {benchmarkManagerVm: vm})
        verify(dialog !== null, "BenchmarkManagerDialog failed to instantiate")
        dialog.open()
        verify(waitForRendering(dialog.contentItem), "dialog never rendered")
        return dialog
    }

    // The dialog and its table share one handle on the (copied) JS double; see BenchmarkManagerDialog.vm.
    function vm(dialog) { return dialog.editorTable.manager }

    function find(dialog, name) { return ItemLookup.findByObjectName(dialog.contentItem, name) }

    function menuItem(menu, name) {
        for (let i = 0; i < menu.count; ++i)
            if (menu.itemAt(i) && menu.itemAt(i).objectName === name) return menu.itemAt(i)
        return null
    }

    function click(dialog, name) {
        const item = find(dialog, name)
        verify(item !== null, "missing control " + name)
        verify(item.visible && item.enabled, name + " must be visible and enabled")
        waitForRendering(dialog.contentItem)
        mouseClick(item)
        return item
    }

    function test_managementWithoutCards() {
        const dialog = openTableDialog()
        verify(dialog.editorTable, "the dialog must host the table editor")
        verify(find(dialog, "treeNode_a") === null, "scenario cards are replaced by table rows")
        tryVerify(() => find(dialog, "cell_a_scenario") !== null)

        // Known and free scenario add, tier add and category add all remain.
        verify(find(dialog, "knownScenarioPicker") !== null)
        find(dialog, "newScenarioField").text = "popcorn"
        click(dialog, "addScenarioButton")
        compare(vm(dialog).addUnplayedScenarioCalls, ["popcorn"])
        find(dialog, "newCategoryField").text = "Aim"
        click(dialog, "addCategoryButton")
        compare(vm(dialog).addCategoryCalls, ["Aim"])

        // Selected rows go to a group as IDs, in one command.
        vm(dialog).selectedEntryIds = ["c", "a"]
        click(dialog, "assignSelectionButton")
        tryCompare(dialog.assignMenu, "visible", true)
        verify(menuItem(dialog.assignMenu, "assignTarget_") !== null, "Uncategorized must be a target")
        verify(menuItem(dialog.assignMenu, "assignTarget_p") !== null, "subcategories must be targets")
        menuItem(dialog.assignMenu, "assignTarget_g1").triggered()
        compare(vm(dialog).assignScenariosCalls.length, 1)
        compare(vm(dialog).assignScenariosCalls[0][0], ["c", "a"])
        compare(vm(dialog).assignScenariosCalls[0][1], "g1")

        vm(dialog).assignResult = {ok: false, error: "A category cannot hold both scenarios and subcategories."}
        click(dialog, "assignSelectionButton")
        tryCompare(dialog.assignMenu, "visible", true)
        menuItem(dialog.assignMenu, "assignTarget_g2").triggered()
        tryCompare(find(dialog, "tableStatusLabel"), "text", "A category cannot hold both scenarios and subcategories.")

        // Row order within its collection.
        verify(dialog.editorTable.focusCell({entryId: "b"}))
        click(dialog, "moveRowUpButton")
        compare(vm(dialog).reorderScenarioCalls, [["b", 0]])
        compare(find(dialog, "moveRowDownButton").enabled, false, "b is already last in its collection")
        click(dialog, "removeScenarioButton")
        compare(vm(dialog).removeScenariosCalls, [["b"]])
        compare(vm(dialog).removeScenarioCalls, [])

        // Rank management from the header, including an unnamed rank.
        verify(dialog.editorTable.focusCell({tierId: "t2"}))
        tryCompare(dialog, "currentTierId", "t2")
        click(dialog, "moveRankLeftButton")
        compare(vm(dialog).reorderTierCalls, [["t2", 0]])
        const rankName = find(dialog, "rankNameField")
        rankName.forceActiveFocus()
        rankName.text = "Silver"
        rankName.editingFinished()
        compare(vm(dialog).renameTierCalls, [["t2", "Silver"]])
        click(dialog, "removeRankButton")
        compare(vm(dialog).removeTierCalls, ["t2"])

        // Category and subcategory management, including empty groups.
        verify(dialog.editorTable.focusCell({groupId: "g2"}))
        tryCompare(dialog, "currentGroupId", "g2")
        click(dialog, "moveGroupUpButton")
        compare(vm(dialog).reorderCategoryCalls, [["g2", 0]])
        verify(dialog.editorTable.focusCell({groupId: "hollow"}))
        tryCompare(dialog, "currentGroupId", "hollow")
        click(dialog, "moveGroupUpButton")
        compare(vm(dialog).reorderSubcategoryCalls, [["hollow", 0]])
        const groupName = find(dialog, "groupNameField")
        groupName.forceActiveFocus()
        groupName.text = "Renamed"
        groupName.editingFinished()
        compare(vm(dialog).renameGroupCalls, [["hollow", "Renamed"]])
        click(dialog, "removeGroupButton")
        compare(vm(dialog).removeCategoryCalls, ["hollow"])

        // The first subcategory under direct rows needs an explicit choice; Cancel changes nothing.
        verify(dialog.editorTable.focusCell({groupId: "g1"}))
        tryCompare(dialog, "currentGroupId", "g1")
        find(dialog, "newSubcategoryField").text = "Static"
        click(dialog, "addSubcategoryButton")
        tryCompare(dialog.subcategoryRelocationPrompt, "visible", true)
        compare(vm(dialog).addSubcategoryRelocatingCalls.length, 0)
        compare(vm(dialog).addSubcategoryCalls.length, 0)
        dialog.subcategoryRelocationPrompt.reject()
        tryCompare(dialog.subcategoryRelocationPrompt, "visible", false)
        compare(vm(dialog).addSubcategoryRelocatingCalls.length, 0)

        find(dialog, "newSubcategoryField").text = "Static"
        click(dialog, "addSubcategoryButton")
        tryCompare(dialog.subcategoryRelocationPrompt, "visible", true)
        dialog.subcategoryRelocationPrompt.relocate("newSubcategory")
        compare(vm(dialog).addSubcategoryRelocatingCalls, [["g1", "Static", "newSubcategory"]])

        // A category that already has subcategories needs no choice.
        verify(dialog.editorTable.focusCell({groupId: "g2"}))
        tryCompare(dialog, "currentGroupId", "g2")
        find(dialog, "newSubcategoryField").text = "Flick"
        click(dialog, "addSubcategoryButton")
        compare(vm(dialog).addSubcategoryCalls, [["g2", "Flick"]])
        compare(dialog.subcategoryRelocationPrompt.visible, false)
    }

    // One Remove over several selected rows is one session command, hence one Undo step.
    function test_removingSelectedRowsIsOneCommand() {
        const dialog = openTableDialog()
        vm(dialog).selectedEntryIds = ["a", "c", "u-res"]

        dialog.removeSelectedScenarios()

        compare(vm(dialog).removeScenariosCalls, [["a", "c", "u-res"]])
        compare(vm(dialog).removeScenarioCalls, [])
        compare(vm(dialog).commandLog, ["removeScenarios"])
    }

    function test_swatchesAndContextMapping() {
        const dialog = openTableDialog({draftFromLibrary: true})
        verify(dialog.editorTable, "the dialog must host the table editor")

        // Rank colour commits only on acceptance, at the rank's ID.
        verify(dialog.editorTable.focusCell({tierId: "t1"}))
        tryCompare(dialog, "currentTierId", "t1")
        const rankSwatch = click(dialog, "rankColorSwatch")
        verify(String(rankSwatch.Accessible.name) !== "", "swatches carry a textual label")
        tryCompare(dialog.colorDialog, "visible", true)
        compare(dialog.colorDialog.targetKind, "tier")
        compare(dialog.colorDialog.targetId, "t1")
        dialog.colorDialog.reject()
        compare(vm(dialog).setTierColorCalls.length, 0, "a cancelled colour choice sends nothing")
        click(dialog, "rankColorSwatch")
        dialog.colorDialog.selectedColor = "#123456"
        dialog.colorDialog.accept()
        tryVerify(() => vm(dialog).setTierColorCalls.length === 1)
        compare(vm(dialog).setTierColorCalls[0][0], "t1")
        compare(String(vm(dialog).setTierColorCalls[0][1]), "#123456")
        compare(ItemLookup.findByObjectName(dialog.contentItem, "rankHeader_t1").headerName, "Gold")

        verify(dialog.editorTable.focusCell({groupId: "g1"}))
        tryCompare(dialog, "currentGroupId", "g1")
        click(dialog, "groupColorSwatch")
        tryCompare(dialog.colorDialog, "visible", true)
        compare(dialog.colorDialog.targetKind, "group")
        dialog.colorDialog.selectedColor = "#654321"
        dialog.colorDialog.accept()
        tryVerify(() => vm(dialog).setGroupColorCalls.length === 1)
        compare(vm(dialog).setGroupColorCalls[0][0], "g1")

        // Uncategorized is neutral: no user group, so no colour control.
        verify(dialog.editorTable.focusCell({entryId: "u-amb", columnKind: 0}))
        tryCompare(dialog, "currentGroupId", "")
        const groupSwatch = find(dialog, "groupColorSwatch")
        verify(groupSwatch === null || !groupSwatch.visible, "Uncategorized offers no colour edit")

        // Contextual mapping for the current row: candidates with metadata, behind the history gate.
        verify(dialog.editorTable.focusCell({entryId: "u-amb"}))
        tryCompare(find(dialog, "matchLabel"), "text", "Ambiguous")
        const candidate = find(dialog, "candidateOption_hash-b")
        verify(candidate !== null, "each candidate is selectable")
        verify(candidate.text.indexOf("hash-b") !== -1 && candidate.text.indexOf("7") !== -1
               && candidate.text.indexOf("2024") !== -1, "candidate metadata is shown: " + candidate.text)
        mouseClick(candidate)
        tryCompare(dialog.retrospectivePrompt, "visible", true)
        compare(vm(dialog).setScenarioHashCalls.length, 0)
        dialog.retrospectivePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)
        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 1)
        compare(vm(dialog).setScenarioHashCalls[0], ["u-amb", "hash-b"])

        verify(dialog.editorTable.focusCell({entryId: "u-res"}))
        tryCompare(find(dialog, "matchLabel"), "text", "Resolved")
        click(dialog, "clearMappingButton")
        tryCompare(dialog.retrospectivePrompt, "visible", true)
        dialog.retrospectivePrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)
        tryVerify(() => vm(dialog).setScenarioHashCalls.length === 2)
        compare(vm(dialog).setScenarioHashCalls[1], ["u-res", ""])
        compare(vm(dialog).clearThresholdCalls.length, 0, "clearing a mapping keeps thresholds")
    }
}
