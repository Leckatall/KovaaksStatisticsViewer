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
        return ItemLookup.findByObjectNamePrefix(panel, "yAxisChoice_", [])
    }

    // A shared-axis card's centre can fall on its name field, which keeps its own clicks.
    function choose(card) {
        mouseClick(find(card, "axisChoiceMarker"))
    }

    function test_rowsShowSharedAxesWithUsageAndIndependentLinesWithoutRenameOrDelete() {
        const panel = makePanel()

        compare(find(panel, "axisNameField_1").text, "Score")
        compare(find(panel, "axisNameField_2").text, "Accuracy")
        compare(find(panel, "axisUsage_1").text, "Used by 5 lines: Score, Expected Final Score, Expected Final (Top 10%), "
                                                + "Expected Final (Last 10 Runs), Expected Final (Whole Run)")
        compare(find(panel, "axisUsage_2").text, "Used by 2 lines: Accuracy, Accuracy (5s)")
        compare(find(panel, "axisNameError_1").visible, false)

        const independent = find(panel, "yAxisChoice_series:7")
        compare(find(independent, "axisChoiceName").text, "Score/s (5s)")
        compare(find(independent, "axisChoiceName").visible, true)
        compare(ItemLookup.findByObjectNamePrefix(independent, "axisNameField_", []).length, 0)
        compare(ItemLookup.findByObjectNamePrefix(independent, "deleteAxis_", []).length, 0)
        compare(ItemLookup.findByObjectNamePrefix(independent, "yAxisChoice_", []).length, 1)
        compare(find(panel, "independentHiddenNote_7").visible, false)
        compare(find(panel, "independentHiddenNote_8").visible, true)
        compare(find(panel, "noIndependentLinesLabel").visible, false)
    }

    function test_everyLineOnASharedAxisSaysSoInsteadOfAnIndependentList() {
        const shared = overview()
        shared.independentLines = []
        const panel = makePanel(makeFakeSettingsVm({axisOverview: shared}))

        compare(find(panel, "noIndependentLinesLabel").visible, true)
        absent(panel, "yAxisChoice_series:7")
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
        compare(checked[0].objectName, "yAxisChoice_axis:1")
        compare(checked[0].Accessible.role, Accessible.RadioButton)
        compare(checked[0].Accessible.checked, true)
        compare(checked[0].Accessible.name, "Score")
        compare(checked[0].Accessible.description, find(panel, "axisUsage_1").text)
        compare(panel.yAxisGroup.checkedButton, checked[0])
        compare(find(panel, "yAxisChoice_series:7").Accessible.name, "Score/s (5s)")
        compare(find(panel, "yAxisChoice_series:8").Accessible.description, find(panel, "independentHiddenNote_8").text)
    }

    function expectOnlyShown(panel, name) {
        const checked = yAxisButtons(panel).filter(button => button.checked)
        compare(checked.map(button => button.objectName), [name])
        compare(panel.yAxisGroup.checkedButton, checked[0])
    }

    function test_theSelectionFollowsTheViewModel() {
        const panel = makeLivePanel()

        choose(find(panel, "yAxisChoice_series:7"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7"])
        expectOnlyShown(panel, "yAxisChoice_series:7")
    }

    // Rows are patched shared axes first, so moving to axis:2 checks it before series:7 is cleared, and
    // the exclusive group unchecks series:7 itself. Choosing series:7 again shows that its `checked: shown`
    // binding outlived that.
    function test_theSelectionCanMoveBackToASharedAxis() {
        const panel = makeLivePanel()
        choose(find(panel, "yAxisChoice_series:7"))

        choose(find(panel, "yAxisChoice_axis:2"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7", "axis:2"])
        expectOnlyShown(panel, "yAxisChoice_axis:2")
        compare(find(panel, "yAxisChoice_series:7").checked, false)

        choose(find(panel, "yAxisChoice_series:7"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["series:7", "axis:2", "series:7"])
        expectOnlyShown(panel, "yAxisChoice_series:7")
    }

    function test_deletingTheShownAxisListsItsLinesAsIndependentAndShowsTheFirst() {
        const panel = makeLivePanel()
        mouseClick(find(panel, "deleteAxis_1"))
        tryCompare(panel.deleteDialog, "opened", true)

        find(panel.deleteDialog.footer, "deleteAxisConfirmButton").clicked()

        tryVerify(() => ItemLookup.findByObjectName(panel, "yAxisChoice_axis:1") === null)
        verify(waitForRendering(panel))
        const order = ["1.0", "1.1", "1.2", "1.3", "1.4", "7", "8"].map(id => find(panel, "yAxisChoice_series:" + id))
        for (let i = 1; i < order.length; ++i)
            verify(order[i - 1].y < order[i].y, order[i - 1].objectName + " sits above " + order[i].objectName
                   + " (" + order.map(row => row.y).join(", ") + ")")
        compare(find(panel, "yAxisChoice_series:1.0").choiceName, "Score")
        expectOnlyShown(panel, "yAxisChoice_series:1.0")
    }

    function test_freedLinesSlotInAmongTheIndependentLinesInDisplayOrder() {
        const panel = makeLivePanel()
        panel.settingsVm.lineOrder = ["7", "2.0", "2.1", "8"]
        mouseClick(find(panel, "deleteAxis_2"))
        tryCompare(panel.deleteDialog, "opened", true)

        find(panel.deleteDialog.footer, "deleteAxisConfirmButton").clicked()

        tryVerify(() => ItemLookup.findByObjectName(panel, "yAxisChoice_axis:2") === null)
        verify(waitForRendering(panel))
        const order = ["7", "2.0", "2.1", "8"].map(id => find(panel, "yAxisChoice_series:" + id))
        for (let i = 1; i < order.length; ++i)
            verify(order[i - 1].y < order[i].y, order[i - 1].objectName + " sits above " + order[i].objectName
                   + " (" + order.map(row => row.y).join(", ") + ")")
        compare(find(panel, "yAxisChoice_series:2.0").choiceName, "Accuracy")
        expectOnlyShown(panel, "yAxisChoice_axis:1")
    }

    function test_choosingAnotherRowStagesItsKey() {
        const panel = makePanel()

        choose(find(panel, "yAxisChoice_axis:2"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["axis:2"])
        // The fake does not republish, so the shown row is still the one the VM reported.
        compare(find(panel, "yAxisChoice_axis:1").checked, true)
        compare(find(panel, "yAxisChoice_axis:2").checked, false)
    }

    function test_choosingTheShownRowDoesNothing() {
        const panel = makePanel()

        choose(find(panel, "yAxisChoice_axis:1"))

        compare(panel.settingsVm.setRunAxisLabelCalls.length, 0)
        compare(find(panel, "yAxisChoice_axis:1").checked, true)
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
        verify(error.mapToItem(panel, 0, 0).y >= field.mapToItem(panel, 0, field.height).y,
               "the error sits under the field")

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
        const oldDelete = find(panel, "deleteAxis_1")
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()
        typeText("Points")

        keyClick(Qt.Key_Tab)

        tryVerify(() => panel.settingsVm.renameAxisCalls.length === 1)
        compare(panel.settingsVm.renameAxisCalls, [["1", "Points"]])
        tryVerify(() => focusedName() === "deleteAxis_1")
        const focused = testCase.Window.window.activeFocusItem
        verify(focused === oldDelete, "the edit keeps the row instead of replacing it")
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
        compare(panel.settingsVm.setRunAxisLabelCalls, [], "Return in a name field chooses nothing")
    }

    // The design's constrained editor width.
    function test_aNarrowCardKeepsDeleteBesideTheName() {
        const narrow = makePanel(makeFakeSettingsVm(), 342)
        const card = find(narrow, "yAxisChoice_axis:1")
        const field = find(narrow, "axisNameField_1")
        const deleteButton = find(narrow, "deleteAxis_1")
        const fieldTop = field.mapToItem(card, 0, 0).y
        const deleteTop = deleteButton.mapToItem(card, 0, 0).y

        verify(deleteTop < fieldTop + field.height && fieldTop < deleteTop + deleteButton.height,
               "Delete shares the name's row")
        verify(deleteButton.mapToItem(card, 0, 0).x >= field.mapToItem(card, field.width, 0).x,
               "Delete sits right of the name")
        fuzzyCompare(field.width + deleteButton.width + card.headerSpacing,
                     card.availableWidth - card.markerSize - card.headerSpacing, 1,
                     "the name field takes the rest of the header")
        verify(deleteButton.mapToItem(narrow, deleteButton.width, 0).x <= narrow.width)
        verify(find(narrow, "axisUsage_1").lineCount > 1, "the usage summary wraps")

        const line = find(narrow, "yAxisChoice_series:7")
        verify(line.mapToItem(narrow, line.width, 0).x <= narrow.width)
    }

    function test_keyboardReachesEveryAction() {
        const panel = makePanel()
        find(panel, "yAxisChoice_axis:1").forceActiveFocus(Qt.TabFocusReason)
        const expected = [
            "yAxisChoice_axis:1", "axisNameField_1", "deleteAxis_1",
            "yAxisChoice_axis:2", "axisNameField_2", "deleteAxis_2",
            "yAxisChoice_axis:3", "axisNameField_3", "deleteAxis_3",
            "newAxisButton", "yAxisChoice_series:7", "yAxisChoice_series:8"
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

        find(panel, "yAxisChoice_axis:2").forceActiveFocus()
        keyClick(Qt.Key_Return)
        find(panel, "yAxisChoice_series:7").forceActiveFocus()
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
        const row = find(panel, "yAxisChoice_axis:1")
        const detailWidth = row.availableWidth - row.markerSize - row.headerSpacing
        fuzzyCompare(find(panel, "axisUsage_1").width, detailWidth, 1, "usage text wraps at the card's detail width")
        const independent = find(panel, "yAxisChoice_series:7")
        const name = find(independent, "axisChoiceName")
        fuzzyCompare(name.width, independent.availableWidth - independent.markerSize - independent.headerSpacing, 1,
                     "the line name takes the card's width beside its marker")
    }

    function test_clickingAnywhereElseOnASharedCardChoosesIt() {
        const panel = makePanel()

        mouseClick(find(panel, "axisUsage_2"))

        compare(panel.settingsVm.setRunAxisLabelCalls, ["axis:2"])
    }

    function test_theNameFieldAndDeleteDoNotChooseTheirAxis() {
        const panel = makePanel()

        mouseClick(find(panel, "axisNameField_2"))
        mouseClick(find(panel, "deleteAxis_3"))

        compare(panel.settingsVm.setRunAxisLabelCalls, [])
        compare(panel.settingsVm.deleteAxisCalls, ["3"])
    }

    function test_deleteIsATrashButtonNamedForItsAxis() {
        const panel = makePanel()
        const deleteButton = find(panel, "deleteAxis_1")

        compare(deleteButton.display, AbstractButton.IconOnly)
        compare(deleteButton.ToolTip.text, "Delete axis")
        compare(deleteButton.Accessible.name, "Delete axis Score")
    }

    function test_clickingARowButtonRightAfterEditingANameStillActs() {
        const panel = makeLivePanel()
        const field = find(panel, "axisNameField_1")
        field.forceActiveFocus()
        field.selectAll()
        typeText("Points")
        const marker = find(find(panel, "yAxisChoice_axis:2"), "axisChoiceMarker")

        mousePress(marker)
        wait(20)
        mouseRelease(marker)

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
        tryVerify(() => ItemLookup.findByObjectName(panel, "yAxisChoice_axis:3") === null, 5000, "the unused axis row is removed")
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
