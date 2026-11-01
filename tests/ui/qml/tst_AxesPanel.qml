import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/series"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "AxesPanelTest"
    when: windowShown
    width: 800
    height: 900
    visible: true

    Component {
        id: externalFixture
        ScrollView {
            id: page
            property var vm
            property alias panel: panel
            width: 700
            height: 250
            contentWidth: availableWidth
            AxesPanel {
                id: panel
                width: page.availableWidth
                settingsVm: page.vm
            }
        }
    }
    function test_thePanelSizesToItsContentInsideThePage() {
        const axes = []
        for (let i = 0; i < 8; ++i)
            axes.push(sharedAxis(String(i), "Axis " + i, []))
        const page = createTemporaryObject(externalFixture, testCase, {
            vm: makeFakeSettingsVm({axisOverview: {sharedAxes: axes, independentLines: []}})
        })
        verify(waitForRendering(page))
        verify(page.panel.implicitHeight > page.height)
        verify(page.contentItem.contentHeight > page.height)
    }

    Component {
        id: panelComponent
        AxesPanel {}
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }

    // Unlike makeFakeSettingsVm()'s plain JS object, axisOverview here is a notifying property, and
    // each mutation republishes it whole, the way the real SettingsViewModel does.
    Component {
        id: liveSettingsVmComponent

        QtObject {
            property var axisOverview: ({shownKey: "", sharedAxes: [], independentLines: []})
            property var renameAxisCalls: []
            property var setRunAxisLabelCalls: []
            property var createAxisCalls: []
            property var deleteAxisCalls: []
            // Like the real VM: trims, and republishes nothing when the stored name does not change.
            function renameAxis(id, text) {
                renameAxisCalls = renameAxisCalls.concat([[id, text]])
                const next = JSON.parse(JSON.stringify(axisOverview))
                for (const axis of next.sharedAxes)
                    if (axis.id === id) {
                        if (axis.name === text.trim())
                            return {succeeded: true, issue: ""}
                        axis.name = text.trim()
                        axis.nameText = text.trim()
                    }
                axisOverview = next
                return {succeeded: true, issue: ""}
            }
            function setRunAxisLabel(key) {
                setRunAxisLabelCalls = setRunAxisLabelCalls.concat([key])
                const next = JSON.parse(JSON.stringify(axisOverview))
                next.shownKey = key
                for (const row of next.sharedAxes.concat(next.independentLines))
                    row.shown = row.key === key
                axisOverview = next
            }
            function createAxis(name) {
                createAxisCalls = createAxisCalls.concat([name])
                const next = JSON.parse(JSON.stringify(axisOverview))
                next.sharedAxes.push({id: "9", key: "axis:9", name: name, nameText: name, nameIssue: "",
                                      lineIds: [], lineNames: [], hasEnabledLine: false, shown: false})
                axisOverview = next
                return {succeeded: true, createdAxisId: "9", issue: ""}
            }
            // Line ids in display order. The real VM lists independent lines in that order, so freed
            // lines can land between existing rows; empty puts them ahead of the existing ones.
            property var lineOrder: []
            // Like the real VM, the axis's lines become independent, and a shown axis hands the
            // selection to its first line.
            function deleteAxis(id) {
                deleteAxisCalls = deleteAxisCalls.concat([id])
                const next = JSON.parse(JSON.stringify(axisOverview))
                const deleted = next.sharedAxes.find(axis => axis.id === id)
                next.sharedAxes = next.sharedAxes.filter(axis => axis.id !== id)
                const freed = deleted.lineIds.map((lineId, i) => ({
                    id: lineId, key: "series:" + lineId, name: deleted.lineNames[i], enabled: true, shown: false
                }))
                if (deleted.shown && freed.length > 0) {
                    freed[0].shown = true
                    next.shownKey = freed[0].key
                }
                next.independentLines = freed.concat(next.independentLines)
                if (lineOrder.length > 0)
                    next.independentLines.sort((a, b) => lineOrder.indexOf(a.id) - lineOrder.indexOf(b.id))
                axisOverview = next
                return {succeeded: true}
            }
        }
    }

    function sharedAxis(id, name, lineNames, overrides) {
        return Object.assign({
            id: id, key: "axis:" + id, name: name, nameText: name, nameIssue: "",
            lineIds: lineNames.map((_, i) => id + "." + i), lineNames: lineNames,
            hasEnabledLine: lineNames.length > 0, shown: false
        }, overrides)
    }

    function independentLine(id, name, enabled) {
        return {id: id, key: "series:" + id, name: name, enabled: enabled, shown: false}
    }

    // Matches images/02-axes.png, plus an unused axis and a disabled independent line.
    function overview() {
        return {
            shownKey: "axis:1",
            sharedAxes: [
                sharedAxis("1", "Score", ["Score", "Expected Final Score", "Expected Final (Top 10%)",
                                          "Expected Final (Last 10 Runs)", "Expected Final (Whole Run)"], {shown: true}),
                sharedAxis("2", "Accuracy", ["Accuracy", "Accuracy (5s)"]),
                sharedAxis("3", "Spare", [])
            ],
            independentLines: [independentLine("7", "Score/s (5s)", true), independentLine("8", "Old line", false)]
        }
    }

    function makeFakeSettingsVm(overrides) {
        return TestDoubles.makeFakeSettingsVm(Object.assign({
            axisOverview: overview(),
            updateSeriesAxisCalls: [],
            updateSeriesAxis: function (lineId, axisId) {
                this.updateSeriesAxisCalls.push([lineId, axisId])
                return {succeeded: true}
            }
        }, overrides))
    }

    function makePanel(vm, width) {
        const page = createTemporaryObject(externalFixture, testCase,
                                           {vm: vm || makeFakeSettingsVm(), width: width || 800, height: 900})
        const panel = page.panel
        verify(panel !== null)
        verify(waitForRendering(panel))
        return panel
    }

    function makeLivePanel(width) {
        const vm = createTemporaryObject(liveSettingsVmComponent, testCase, {axisOverview: overview()})
        return makePanel(vm, width)
    }

    function find(panel, name) {
        const found = ItemLookup.findByObjectName(panel, name)
        verify(found !== null, "no item " + name)
        return found
    }

    function absent(panel, name) {
        compare(ItemLookup.findByObjectName(panel, name), null, name + " should not exist")
    }

    function focusedName() {
        const item = testCase.Window.window.activeFocusItem
        return item ? item.objectName : ""
    }

    function typeText(text) {
        for (const character of text)
            keyClick(character)
    }

    function yAxisButtons(panel) {
        return ItemLookup.findByObjectNamePrefix(panel, "showOnYAxis_", [])
    }

    function test_rowsShowSharedAxesWithUsageAndIndependentLinesWithoutRenameOrDelete() {
        const panel = makePanel()

        compare(find(panel, "axisNameField_1").text, "Score")
        compare(find(panel, "axisNameField_2").text, "Accuracy")
        compare(find(panel, "axisUsage_1").text, "Used by 5 lines: Score, Expected Final Score, Expected Final (Top 10%), "
                                                + "Expected Final (Last 10 Runs), Expected Final (Whole Run)")
        compare(find(panel, "axisUsage_2").text, "Used by 2 lines: Accuracy, Accuracy (5s)")
        compare(find(panel, "axisNameError_1").visible, false)

        const independent = find(panel, "independentRow_7")
        compare(find(panel, "independentName_7").text, "Score/s (5s)")
        compare(ItemLookup.findByObjectNamePrefix(independent, "axisNameField_", []).length, 0)
        compare(ItemLookup.findByObjectNamePrefix(independent, "deleteAxis_", []).length, 0)
        compare(ItemLookup.findByObjectNamePrefix(independent, "showOnYAxis_", []).length, 1)
        compare(find(panel, "independentHiddenNote_7").visible, false)
        compare(find(panel, "independentHiddenNote_8").visible, true)
        compare(find(panel, "noIndependentLinesLabel").visible, false)
    }

    function test_everyLineOnASharedAxisSaysSoInsteadOfAnIndependentList() {
        const shared = overview()
        shared.independentLines = []
        const panel = makePanel(makeFakeSettingsVm({axisOverview: shared}))

        compare(find(panel, "noIndependentLinesLabel").visible, true)
        absent(panel, "independentRow_7")
    }

    function test_anUnusedAxisSaysItIsNotAssignedAndDeletesWithoutConfirmation() {
        const panel = makePanel()
        compare(find(panel, "axisUsage_3").text, "Not assigned to any lines.")
        const deleteButton = find(panel, "deleteAxis_3")

        mouseClick(deleteButton)

        compare(panel.settingsVm.deleteAxisCalls, ["3"])
        compare(panel.deleteDialog.opened, false)
    }

    function test_exactlyOneRowIsShownOnTheYAxisAcrossBothGroups() {
        const panel = makePanel()
        const checked = yAxisButtons(panel).filter(button => button.checked)

        compare(yAxisButtons(panel).length, 5)
        compare(checked.length, 1)
        compare(checked[0].objectName, "showOnYAxis_axis:1")
        compare(checked[0].Accessible.role, Accessible.RadioButton)
        compare(checked[0].Accessible.checked, true)
        compare(checked[0].Accessible.name, "Show Score on Y-axis")
        compare(panel.yAxisGroup.checkedButton, checked[0])
        compare(find(panel, "showOnYAxis_series:7").Accessible.name, "Show Score/s (5s) on Y-axis")
    }

    function expectOnlyShown(panel, name) {
        const checked = yAxisButtons(panel).filter(button => button.checked)
        compare(checked.map(button => button.objectName), [name])
        compare(panel.yAxisGroup.checkedButton, checked[0])
    }

    function test_theSelectionFollowsTheViewModel() {
        const panel = makeLivePanel()

        mouseClick(find(panel, "showOnYAxis_series:7"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7"])
        expectOnlyShown(panel, "showOnYAxis_series:7")
    }

    // Rows are patched shared axes first, so moving to axis:2 checks it before series:7 is cleared, and
    // the exclusive group unchecks series:7 itself. Choosing series:7 again shows that its `checked: shown`
    // binding outlived that.
    function test_theSelectionCanMoveBackToASharedAxis() {
        const panel = makeLivePanel()
        mouseClick(find(panel, "showOnYAxis_series:7"))

        mouseClick(find(panel, "showOnYAxis_axis:2"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7", "axis:2"])
        expectOnlyShown(panel, "showOnYAxis_axis:2")
        compare(find(panel, "showOnYAxis_series:7").text, "Show on Y-axis")

        mouseClick(find(panel, "showOnYAxis_series:7"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7", "axis:2", "series:7"])
        expectOnlyShown(panel, "showOnYAxis_series:7")
    }

    function test_deletingTheShownAxisListsItsLinesAsIndependentAndShowsTheFirst() {
        const panel = makeLivePanel()
        mouseClick(find(panel, "deleteAxis_1"))
        tryCompare(panel.deleteDialog, "opened", true)

        find(panel.deleteDialog.footer, "deleteAxisConfirmButton").clicked()

        tryVerify(() => ItemLookup.findByObjectName(panel, "axisRow_1") === null)
        verify(waitForRendering(panel))
        const order = ["1.0", "1.1", "1.2", "1.3", "1.4", "7", "8"].map(id => find(panel, "independentRow_" + id))
        for (let i = 1; i < order.length; ++i)
            verify(order[i - 1].y < order[i].y, order[i - 1].objectName + " sits above " + order[i].objectName
                   + " (" + order.map(row => row.y).join(", ") + ")")
        compare(find(panel, "independentName_1.0").text, "Score")
        expectOnlyShown(panel, "showOnYAxis_series:1.0")
    }

    function test_freedLinesSlotInAmongTheIndependentLinesInDisplayOrder() {
        const panel = makeLivePanel()
        panel.settingsVm.lineOrder = ["7", "2.0", "2.1", "8"]
        mouseClick(find(panel, "deleteAxis_2"))
        tryCompare(panel.deleteDialog, "opened", true)

        find(panel.deleteDialog.footer, "deleteAxisConfirmButton").clicked()

        tryVerify(() => ItemLookup.findByObjectName(panel, "axisRow_2") === null)
        verify(waitForRendering(panel))
        const order = ["7", "2.0", "2.1", "8"].map(id => find(panel, "independentRow_" + id))
        for (let i = 1; i < order.length; ++i)
            verify(order[i - 1].y < order[i].y, order[i - 1].objectName + " sits above " + order[i].objectName
                   + " (" + order.map(row => row.y).join(", ") + ")")
        compare(find(panel, "independentName_2.0").text, "Accuracy")
        expectOnlyShown(panel, "showOnYAxis_axis:1")
    }

    function test_choosingAnotherRowStagesItsKey() {
        const panel = makePanel()

        mouseClick(find(panel, "showOnYAxis_axis:2"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["axis:2"])
        // The fake does not republish, so the shown row is still the one the VM reported.
        compare(find(panel, "showOnYAxis_axis:1").checked, true)
        compare(find(panel, "showOnYAxis_axis:2").checked, false)
    }

    function test_choosingTheShownRowDoesNothing() {
        const panel = makePanel()

        mouseClick(find(panel, "showOnYAxis_axis:1"))

        compare(panel.settingsVm.setRunAxisLabelCalls.length, 0)
        compare(find(panel, "showOnYAxis_axis:1").checked, true)
    }

    function test_aRowWithNoVisibleLinesExplainsTheFallback() {
        const hidden = overview()
        hidden.sharedAxes[1].hasEnabledLine = false
        const panel = makePanel(makeFakeSettingsVm({axisOverview: hidden}))

        const note = find(panel, "axisHiddenNote_2")
        compare(note.visible, true)
        compare(find(panel, "axisHiddenNote_1").visible, false)
    }

    function test_editingANameCommitsOnceOnEditingFinished() {
        const panel = makePanel()
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()

        typeText("Points")
        wait(0)
        compare(panel.settingsVm.renameAxisCalls.length, 0)
        keyClick(Qt.Key_Tab)

        tryVerify(() => panel.settingsVm.renameAxisCalls.length > 0)
        wait(50)
        compare(panel.settingsVm.renameAxisCalls, [["1", "Points"]])
    }

    function test_leavingAnUnchangedNameCommitsNothing() {
        const panel = makePanel()
        find(panel, "axisNameField_2").forceActiveFocus()

        keyClick(Qt.Key_Tab)

        wait(50)
        compare(panel.settingsVm.renameAxisCalls.length, 0)
    }

    function test_aRejectedNameStaysInTheFieldWithItsErrorText() {
        const rejected = overview()
        rejected.sharedAxes[1].nameText = "score"
        rejected.sharedAxes[1].nameIssue = "axisNameDuplicate"
        const panel = makePanel(makeFakeSettingsVm({axisOverview: rejected}))

        const field = find(panel, "axisNameField_2")
        const error = find(panel, "axisNameError_2")
        compare(field.text, "score")
        compare(error.visible, true)
        verify(error.y >= field.y + field.height, "the error sits under the field")

        field.forceActiveFocus()
        keyClick(Qt.Key_Tab)
        wait(50)
        compare(panel.settingsVm.renameAxisCalls.length, 0)
    }

    function test_deletingAnAssignedAxisAsksFirstAndListsItsLines() {
        const panel = makePanel()

        mouseClick(find(panel, "deleteAxis_2"))

        const dialog = panel.deleteDialog
        tryCompare(dialog, "opened", true)
        compare(dialog.title, "Delete \"Accuracy\" axis?")
        compare(find(dialog.contentItem, "deleteAxisSummary").text, "2 lines will switch to Independent:")
        verify(ItemLookup.findByText(dialog.contentItem, "• Accuracy") !== null)
        verify(ItemLookup.findByText(dialog.contentItem, "• Accuracy (5s)") !== null)
        compare(panel.settingsVm.deleteAxisCalls.length, 0)
    }

    function test_cancellingTheDeleteConfirmationChangesNothing() {
        const panel = makePanel()
        mouseClick(find(panel, "deleteAxis_2"))
        const dialog = panel.deleteDialog
        tryCompare(dialog, "opened", true)

        find(dialog.footer, "deleteAxisCancelButton").clicked()

        tryCompare(dialog, "opened", false)
        compare(panel.settingsVm.deleteAxisCalls.length, 0)
        compare(panel.settingsVm.setRunAxisLabelCalls.length, 0)
    }

    function test_confirmingDeletesTheAxis() {
        const panel = makePanel()
        mouseClick(find(panel, "deleteAxis_1"))
        const dialog = panel.deleteDialog
        tryCompare(dialog, "opened", true)

        find(dialog.footer, "deleteAxisConfirmButton").clicked()

        compare(panel.settingsVm.deleteAxisCalls, ["1"])
        tryCompare(dialog, "opened", false)
    }

    function test_newAxisFromAxesCreatesItUnassigned() {
        const panel = makePanel()

        mouseClick(find(panel, "newAxisButton"))
        const dialog = panel.newAxisDialog
        tryCompare(dialog, "opened", true)
        dialog.nameField.text = "Custom"
        find(dialog.footer, "newAxisCreateButton").clicked()

        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
        compare(dialog.settingsVm.updateSeriesAxisCalls.length, 0)
        tryCompare(dialog, "opened", false)
    }

    function test_focusStaysPutAfterARename() {
        const panel = makeLivePanel()
        const oldShow = find(panel, "showOnYAxis_axis:1")
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()
        typeText("Points")

        keyClick(Qt.Key_Tab)

        tryVerify(() => panel.settingsVm.renameAxisCalls.length === 1)
        compare(panel.settingsVm.renameAxisCalls, [["1", "Points"]])
        tryVerify(() => focusedName() === "showOnYAxis_axis:1")
        const focused = testCase.Window.window.activeFocusItem
        verify(focused === oldShow, "the edit keeps the row instead of replacing it")
        compare(find(panel, "axisNameField_1").text, "Points")

        const accuracy = find(panel, "axisNameField_2")
        accuracy.forceActiveFocus()
        accuracy.selectAll()
        typeText("Hits")
        keyClick(Qt.Key_Return)

        tryVerify(() => panel.settingsVm.renameAxisCalls.length === 2)
        compare(panel.settingsVm.renameAxisCalls, [["1", "Points"], ["2", "Hits"]])
        tryVerify(() => focusedName() === "axisNameField_2")
        verify(testCase.Window.window.activeFocusItem === accuracy)
        compare(accuracy.text, "Hits")
    }

    function test_narrowWidthMovesActionsBelowTheName() {
        const wide = makePanel(makeFakeSettingsVm(), 800)
        const wideField = find(wide, "axisNameField_1")
        const wideShow = find(wide, "showOnYAxis_axis:1")
        const wideRow = find(wide, "axisRow_1")
        verify(wideShow.mapToItem(wideRow, 0, 0).y < wideField.mapToItem(wideRow, 0, 0).y + wideField.height,
               "wide rows keep the actions beside the name")

        const narrow = makePanel(makeFakeSettingsVm(), 400)
        const narrowField = find(narrow, "axisNameField_1")
        const narrowRow = find(narrow, "axisRow_1")
        const narrowShow = find(narrow, "showOnYAxis_axis:1")
        const narrowDelete = find(narrow, "deleteAxis_1")
        const fieldBottom = narrowField.mapToItem(narrowRow, 0, 0).y + narrowField.height
        verify(narrowShow.mapToItem(narrowRow, 0, 0).y >= fieldBottom, "narrow rows put Show below the name")
        verify(narrowDelete.mapToItem(narrowRow, 0, 0).y >= fieldBottom, "narrow rows put Delete below the name")
        fuzzyCompare(narrowField.width, narrowRow.availableWidth, 1, "the name field takes the whole narrow row")
        verify(narrowDelete.mapToItem(narrow, narrowDelete.width, 0).x <= narrow.width)

        const besideName = find(wide, "independentName_7")
        const besideShow = find(wide, "showOnYAxis_series:7")
        const wideLine = find(wide, "independentRow_7")
        verify(besideShow.mapToItem(wideLine, 0, 0).y < besideName.mapToItem(wideLine, 0, 0).y + besideName.height,
               "wide independent rows keep Show beside the line name")
        const lineName = find(narrow, "independentName_7")
        const lineShow = find(narrow, "showOnYAxis_series:7")
        const narrowLine = find(narrow, "independentRow_7")
        verify(lineShow.mapToItem(narrowLine, 0, 0).y >= lineName.mapToItem(narrowLine, 0, 0).y + lineName.height,
               "narrow independent rows put Show below the line name")
        verify(lineShow.mapToItem(narrow, lineShow.width, 0).x <= narrow.width)
    }

    function test_keyboardReachesEveryAction() {
        const panel = makePanel()
        find(panel, "axisNameField_1").forceActiveFocus()
        const expected = [
            "axisNameField_1", "showOnYAxis_axis:1", "deleteAxis_1",
            "axisNameField_2", "showOnYAxis_axis:2", "deleteAxis_2",
            "axisNameField_3", "showOnYAxis_axis:3", "deleteAxis_3",
            "newAxisButton", "showOnYAxis_series:7", "showOnYAxis_series:8"
        ]
        const visited = [focusedName()]
        for (let i = 1; i < expected.length; ++i) {
            keyClick(Qt.Key_Tab)
            visited.push(focusedName())
        }

        compare(visited, expected)

        keyClick(Qt.Key_Space)
        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:8"])
    }

    function test_enterActivatesEveryAction() {
        const panel = makePanel()

        find(panel, "showOnYAxis_axis:2").forceActiveFocus()
        keyClick(Qt.Key_Return)
        find(panel, "showOnYAxis_series:7").forceActiveFocus()
        keyClick(Qt.Key_Enter)
        compare(panel.settingsVm.setRunAxisLabelCalls, ["axis:2", "series:7"])

        find(panel, "deleteAxis_3").forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(panel.settingsVm.deleteAxisCalls, ["3"])

        find(panel, "deleteAxis_2").forceActiveFocus()
        keyClick(Qt.Key_Return)
        tryCompare(panel.deleteDialog, "opened", true)
        keyClick(Qt.Key_Escape)
        tryCompare(panel.deleteDialog, "opened", false)

        find(panel, "newAxisButton").forceActiveFocus()
        keyClick(Qt.Key_Return)
        tryCompare(panel.newAxisDialog, "opened", true)
    }

    // Enter acts once, on release like Space: a held key's repeated presses must not run on into
    // whatever takes focus next (here + New axis…, then its empty name field).
    function test_holdingEnterActsOnceOnRelease() {
        const panel = makeLivePanel()
        find(panel, "deleteAxis_3").forceActiveFocus()

        keyPress(Qt.Key_Return)
        keyPress(Qt.Key_Return)

        compare(panel.settingsVm.deleteAxisCalls, [])
        keyRelease(Qt.Key_Return)
        compare(panel.settingsVm.deleteAxisCalls, ["3"])
        tryVerify(() => focusedName() === "newAxisButton")
        compare(panel.newAxisDialog.opened, false)
    }

    function test_enterActivatesTheDeleteConfirmationButtons() {
        const panel = makeLivePanel()
        find(panel, "deleteAxis_2").forceActiveFocus()
        keyClick(Qt.Key_Return)
        tryCompare(panel.deleteDialog, "opened", true)
        tryVerify(() => focusedName() === "deleteAxisCancelButton")

        keyClick(Qt.Key_Return)

        tryCompare(panel.deleteDialog, "opened", false)
        compare(panel.settingsVm.deleteAxisCalls, [])
        tryVerify(() => focusedName() === "deleteAxis_2")

        keyClick(Qt.Key_Return)
        tryCompare(panel.deleteDialog, "opened", true)
        tryVerify(() => focusedName() === "deleteAxisCancelButton")
        keyClick(Qt.Key_Tab)
        compare(focusedName(), "deleteAxisConfirmButton")

        keyClick(Qt.Key_Enter)

        compare(panel.settingsVm.deleteAxisCalls, ["2"])
        tryCompare(panel.deleteDialog, "opened", false)
    }

    function test_rowsFillTheirWidth() {
        const panel = makePanel()
        const row = find(panel, "axisRow_1")
        fuzzyCompare(find(panel, "axisUsage_1").width, row.availableWidth, 1, "usage text wraps at the row width")
        const independent = find(panel, "independentRow_7")
        const show = find(panel, "showOnYAxis_series:7")
        const name = find(panel, "independentName_7")
        verify(name.width > independent.availableWidth / 2, "the line name gets the room beside its button")
        verify(name.mapToItem(independent, name.width, 0).x <= show.mapToItem(independent, 0, 0).x,
               "the line name stays clear of its button")
    }

    function test_clickingARowButtonRightAfterEditingANameStillActs() {
        const panel = makeLivePanel()
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()
        typeText("Points")
        const show = find(panel, "showOnYAxis_axis:2")

        mousePress(show)
        wait(20)
        mouseRelease(show)

        compare(panel.settingsVm.renameAxisCalls, [["1", "Points"]])
        compare(panel.settingsVm.setRunAxisLabelCalls, ["axis:2"])
    }

    function test_aNameThatOnlyGainsWhitespaceShowsTheStoredName() {
        const panel = makeLivePanel()
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()
        typeText(" Score ")

        keyClick(Qt.Key_Tab)

        tryCompare(field, "text", "Score")
    }

    function test_nameFieldsCarryTheirAxisAndErrorForAssistiveTechnology() {
        const rejected = overview()
        rejected.sharedAxes[1].nameText = "score"
        rejected.sharedAxes[1].nameIssue = "axisNameDuplicate"
        const panel = makePanel(makeFakeSettingsVm({axisOverview: rejected}))

        compare(find(panel, "axisNameField_1").Accessible.name, "Name of axis Score")
        compare(find(panel, "axisNameField_1").Accessible.description, "")
        compare(find(panel, "axisNameField_2").Accessible.description, "Another axis already has this name.")
    }

    function test_theDeleteConfirmationWorksFromTheKeyboard() {
        const panel = makeLivePanel()
        find(panel, "deleteAxis_2").forceActiveFocus()

        keyClick(Qt.Key_Space)

        tryCompare(panel.deleteDialog, "opened", true)
        tryVerify(() => focusedName() === "deleteAxisCancelButton")
        keyClick(Qt.Key_Tab)
        compare(focusedName(), "deleteAxisConfirmButton")
        keyClick(Qt.Key_Space)

        compare(panel.settingsVm.deleteAxisCalls, ["2"])
        tryCompare(panel.deleteDialog, "opened", false)
        tryVerify(() => focusedName() === "newAxisButton")
    }

    function test_escapeCancelsTheDeleteConfirmationAndReturnsToItsButton() {
        const panel = makeLivePanel()
        find(panel, "deleteAxis_1").forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(panel.deleteDialog, "opened", true)

        keyClick(Qt.Key_Escape)

        tryCompare(panel.deleteDialog, "opened", false)
        compare(panel.settingsVm.deleteAxisCalls.length, 0)
        tryVerify(() => focusedName() === "deleteAxis_1")
    }

    function test_deletingAnUnusedAxisMovesFocusToNewAxis() {
        const panel = makeLivePanel()
        find(panel, "deleteAxis_3").forceActiveFocus()

        keyClick(Qt.Key_Space)

        compare(panel.settingsVm.deleteAxisCalls, ["3"])
        tryVerify(() => ItemLookup.findByObjectName(panel, "axisRow_3") === null, 5000, "the unused axis row is removed")
        tryVerify(() => focusedName() === "newAxisButton")
    }

    function test_creatingAnAxisFromTheKeyboardFocusesItsName() {
        const panel = makeLivePanel()
        find(panel, "newAxisButton").forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(panel.newAxisDialog, "opened", true)
        tryVerify(() => focusedName() === "newAxisNameField")

        typeText("Custom")
        keyClick(Qt.Key_Return)

        compare(panel.settingsVm.createAxisCalls, ["Custom"])
        tryCompare(panel.newAxisDialog, "opened", false)
        tryVerify(() => focusedName() === "axisNameField_9")
    }

    function test_cancellingNewAxisReturnsToItsButton() {
        const panel = makeLivePanel()
        find(panel, "newAxisButton").forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(panel.newAxisDialog, "opened", true)

        keyClick(Qt.Key_Escape)

        tryCompare(panel.newAxisDialog, "opened", false)
        tryVerify(() => focusedName() === "newAxisButton")
    }

    function test_focusAxisNameShowsThatField() {
        const panel = makePanel()

        panel.focusAxisName("2")

        tryVerify(() => focusedName() === "axisNameField_2")
    }
}
