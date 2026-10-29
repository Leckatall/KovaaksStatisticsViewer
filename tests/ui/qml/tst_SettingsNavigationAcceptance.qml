import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "SettingsNavigationAcceptanceTest"
    when: windowShown
    visible: true
    width: 700
    height: 500

    Component { id: dialogComponent; SettingsDialog {} }
    Component { id: vmComponent; FakeQuickSettingsVm {} }

    function makeDialog(graph) {
        const vm = createTemporaryObject(vmComponent, testCase)
        const session = {profileBuildInProgress: false, generateProfile: function() {}}
        const dialog = createTemporaryObject(dialogComponent, testCase, {settingsVm: vm, sessionVm: session})
        dialog.openGraph(graph)
        dialog.width = dialog.floorWidth
        dialog.height = dialog.floorHeight
        verify(waitForRendering(dialog.contentItem))
        return {dialog: dialog, vm: vm}
    }

    function find(dialog, name) {
        return ItemLookup.findByObjectName(dialog.contentItem, name) || ItemLookup.findByObjectName(dialog.footer, name)
    }

    function insideWindow(dialog, item) {
        const p = item.mapToItem(null, 0, 0)
        return p.x >= -1 && p.y >= -1 && p.x + item.width <= dialog.width + 1
                && p.y + item.height <= dialog.height + 1
    }

    function test_atTheFloorEverySidebarEntryHeaderAndFooterIsReachable() {
        const {dialog} = makeDialog("scenarioHistory")
        for (const name of ["graphPageTitle", "graphResetButton", "settingsSaveButton", "settingsCancelButton"]) {
            const item = find(dialog, name)
            verify(item !== null, name)
            verify(insideWindow(dialog, item), name)
        }
        const list = find(dialog, "categoryList")
        let flick = list.parent
        while (flick && flick.contentY === undefined)
            flick = flick.parent
        verify(flick !== null, "sidebar has a scrolling viewport")
        for (const key of ["profile", "run", "dailyPlaytime", "scenarioHistory", "scenarioHistory_dataRange",
                           "scenarioHistory_seriesStyle", "scenarioHistory_axes", "benchmarkRank", "benchmarkPlaytime"]) {
            const item = find(dialog, "settingsNav_" + key)
            verify(item !== null, key)
            const p = item.mapToItem(list, 0, 0)
            flick.contentY = Math.max(0, Math.min(p.y, flick.contentHeight - flick.height))
            wait(0)
            const visible = item.mapToItem(flick, 0, 0)
            verify(visible.y >= -1 && visible.y + item.height <= flick.height + 1, key)
        }
        const scroller = find(dialog, "graphEditorScroll")
        verify(scroller.contentHeight > scroller.height)
        scroller.contentItem.contentY = scroller.contentHeight - scroller.height
        verify(waitForRendering(dialog.contentItem))
        const last = find(dialog, "graphField_axes_counts_fitMode")
        const p = last.mapToItem(scroller, 0, 0)
        verify(p.y >= -1 && p.y + last.height <= scroller.height + 1)
    }

    function test_atALargerTextScaleNoSidebarEntryOrHeaderClips() {
        const {dialog, vm} = makeDialog("scenarioHistory")
        vm.changedGraphs = ["scenarioHistory"]
        const font = dialog.font
        font.pixelSize = Math.round(font.pixelSize > 0 ? font.pixelSize * 2 : 28)
        dialog.font = font
        verify(waitForRendering(dialog.contentItem))
        const list = find(dialog, "categoryList")
        const entries = ItemLookup.findByObjectNamePrefix(list, "settingsNav_", [])
        for (const entry of entries) {
            if (entry.text === undefined)
                continue
            const label = entry.contentItem
            verify(label !== null, entry.objectName)
            verify(!label.truncated || label.elide !== Text.ElideNone, entry.objectName)
            const right = label.mapToItem(list, label.width, 0).x
            verify(right <= list.width + 1, entry.objectName)
        }
        const title = find(dialog, "graphPageTitle")
        const reset = find(dialog, "graphResetButton")
        verify(title.mapToItem(null, title.width, 0).x <= reset.mapToItem(null, 0, 0).x + 1)
    }

    function test_tabCyclesSidebarPageSaveCancelAndBack() {
        const {dialog, vm} = makeDialog("dailyPlaytime")
        vm.pendingChanges = true
        const cancel = find(dialog, "settingsCancelButton")
        cancel.forceActiveFocus()
        tryVerify(() => cancel.activeFocus)
        const names = []
        for (let i = 0; i < 80; ++i) {
            keyClick(Qt.Key_Tab)
            const focused = dialog.activeFocusItem
            verify(focused !== null)
            names.push(focused.objectName)
            if (focused === cancel)
                break
        }
        compare(names[0], "settingsNav_profile")
        const resetIndex = names.indexOf("graphResetButton")
        const firstFieldIndex = names.indexOf("graphField_dataRange_range")
        const saveIndex = names.indexOf("settingsSaveButton")
        verify(resetIndex > 0)
        verify(firstFieldIndex > resetIndex)
        verify(saveIndex > firstFieldIndex)
        compare(names[names.length - 1], "settingsCancelButton")
        for (let i = 0; i < resetIndex; ++i)
            verify(names[i].startsWith("settingsNav_"), names[i])
    }
}
