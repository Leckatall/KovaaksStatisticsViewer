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
        verify(dialog.editorTable)
        verify(find(dialog, "treeNode_a") === null)
        tryVerify(() => find(dialog, "cell_a_scenario") !== null)

        click(dialog, "addScenarioPlus")
        compare(vm(dialog).addUnplayedScenarioCalls, [""])
        click(dialog, "addCategoryPlus")
        const categoryName = find(dialog, "categoryCreationName")
        categoryName.text = "Aim"
        keyClick(Qt.Key_Return)
        compare(vm(dialog).addCategoryCalls, ["Aim"])

        vm(dialog).selectedEntryIds = ["c", "a"]
        dialog.assignSelection("g1")
        compare(vm(dialog).assignScenariosCalls, [[["c", "a"], "g1"]])
        verify(dialog.assignTargets().some(target => target.id === "p"))
        verify(!dialog.assignTargets().some(target => target.id === "g2"),
               "a category with subcategories cannot receive scenarios")

        verify(dialog.editorTable.focusCell({entryId: "b"}))
        dialog.moveCurrentRow(-1)
        compare(vm(dialog).reorderScenarioCalls, [["b", 0]])
        dialog.removeSelectedScenarios()
        compare(vm(dialog).removeScenariosCalls, [["b"]])

        verify(dialog.editorTable.focusCell({tierId: "t2"}))
        dialog.moveCurrentRankBy(-1)
        compare(vm(dialog).reorderTierCalls, [["t2", 0]])
        verify(dialog.editorTable.beginRankRename("t2"))
        const rankName = find(dialog, "rankHeaderEditor")
        tryVerify(() => rankName !== null)
        rankName.text = "Silver"
        keyClick(Qt.Key_Return)
        compare(vm(dialog).renameTierCalls, [["t2", "Silver"]])
        dialog.removeCurrentRank()
        compare(vm(dialog).removeTierCalls, ["t2"])

        verify(dialog.editorTable.focusCell({groupId: "g2"}))
        dialog.moveCurrentGroup(-1)
        compare(vm(dialog).reorderCategoryCalls, [["g2", 0]])
        verify(dialog.editorTable.beginSubcategoryCreation("g1"))
        const subcategoryName = find(dialog, "subcategoryCreationField")
        tryVerify(() => subcategoryName !== null && subcategoryName.visible)
        subcategoryName.text = "Static"
        keyClick(Qt.Key_Return)
        tryCompare(dialog.subcategoryRelocationPrompt, "visible", true)
        compare(vm(dialog).addSubcategoryCalls.length, 0)
        dialog.subcategoryRelocationPrompt.relocate("newSubcategory")
        compare(vm(dialog).addSubcategoryRelocatingCalls, [["g1", "Static", "newSubcategory"]])
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

    function test_emptyGroupCanBeRenamedFromItsChip() {
        const dialog = openTableDialog()
        verify(dialog.editorTable.beginGroupRename("gE"))
        const field = find(dialog, "emptyGroupRenameField_gE")
        verify(field !== null && field.visible)
        field.text = "New group"
        keyClick(Qt.Key_Return)
        compare(vm(dialog).renameGroupCalls, [["gE", "New group"]])
    }

    function test_groupAndRankHeadersOpenContextMenus() {
        const dialog = openTableDialog()
        const group = find(dialog, "groupListItem_gE")
        verify(group !== null)
        mouseClick(group, group.width / 2, group.height / 2, Qt.RightButton)
        tryVerify(() => dialog.groupContextMenu.opened)
        compare(dialog.currentGroupId, "gE")
        dialog.groupContextMenu.close()

        const rank = find(dialog, "rankHeader_t1")
        verify(rank !== null)
        mouseClick(rank, rank.width / 2, rank.height / 2, Qt.RightButton)
        tryVerify(() => dialog.rankContextMenu.opened)
        compare(dialog.currentTierId, "t1")
    }

    function test_swatchesAndContextMapping() {
        const dialog = openTableDialog({draftFromLibrary: true})
        verify(dialog.editorTable, "the dialog must host the table editor")

        // Rank colour commits only on acceptance, at the rank's ID.
        verify(dialog.editorTable.focusCell({tierId: "t1"}))
        tryCompare(dialog, "currentTierId", "t1")
        const rankSwatch = click(dialog, "rankSwatch_t1")
        verify(String(rankSwatch.Accessible.name) !== "", "swatches carry a textual label")
        tryCompare(dialog.colorDialog, "visible", true)
        compare(dialog.colorDialog.targetKind, "tier")
        compare(dialog.colorDialog.targetId, "t1")
        dialog.colorDialog.reject()
        compare(vm(dialog).setTierColorCalls.length, 0, "a cancelled colour choice sends nothing")
        click(dialog, "rankSwatch_t1")
        dialog.colorDialog.selectedColor = "#123456"
        dialog.colorDialog.accept()
        tryVerify(() => vm(dialog).setTierColorCalls.length === 1)
        compare(vm(dialog).setTierColorCalls[0][0], "t1")
        compare(String(vm(dialog).setTierColorCalls[0][1]), "#123456")
        compare(ItemLookup.findByObjectName(dialog.contentItem, "rankHeader_t1").headerName, "Gold")

        verify(dialog.editorTable.focusCell({groupId: "g1"}))
        tryCompare(dialog, "currentGroupId", "g1")
        click(dialog, "groupSwatch_g1")
        tryCompare(dialog.colorDialog, "visible", true)
        compare(dialog.colorDialog.targetKind, "group")
        dialog.colorDialog.selectedColor = "#654321"
        dialog.colorDialog.accept()
        tryVerify(() => vm(dialog).setGroupColorCalls.length === 1)
        compare(vm(dialog).setGroupColorCalls[0][0], "g1")

        // Uncategorized is neutral: no user group, so no colour control.
        verify(dialog.editorTable.focusCell({entryId: "u-amb", columnKind: 0}))
        tryCompare(dialog, "currentGroupId", "")
        const groupSwatch = ItemLookup.findByObjectName(find(dialog, "cell_u-amb_category"), "groupSwatch_")
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
