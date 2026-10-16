import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

// QUAL-01 and QUAL-02 for the quick-settings popup: every control is named, errors and limits reach
// the control they belong to, the keyboard visits everything and can always leave, and nothing is
// told by colour alone.
TestCase {
    id: testCase
    name: "GraphAccessibilityTest"
    when: windowShown
    width: 700
    height: 600
    visible: true

    FakeQuickSettingsVm { id: fakeVm }

    Component {
        id: gearComponent
        GraphGear {
            x: 600
            y: 20
        }
    }

    readonly property var allKeys: ["run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"]

    function init() {
        fakeVm.patches = []
        fakeVm.rejectWith = []
        fakeVm.quickSettings = ({
            run: fakeVm.entry({hasRange: false, hasLatestRuns: false, hasWindow: false}),
            dailyPlaytime: fakeVm.entry({hasWindow: true, window: 3}),
            scenarioHistory: fakeVm.entry({hasLatestRuns: true, markers: true}),
            benchmarkRank: fakeVm.entry({}),
            benchmarkPlaytime: fakeVm.entry({hasWindow: true, window: 3})
        })
    }

    function makeGear(key) {
        const gear = createTemporaryObject(gearComponent, testCase, {graphKey: key, settingsVm: fakeVm})
        verify(gear !== null)
        return gear
    }

    function openPopup(gear) {
        mouseClick(gear)
        tryVerify(() => gear.popup.opened, 2000, "popup never opened")
        verify(waitForRendering(gear.popup.contentItem))
    }

    function control(gear, name) {
        return ItemLookup.findByObjectName(gear.popup.contentItem, name)
    }

    // The objectName of the control holding keyboard focus, however deep the focused item is.
    function focusedName() {
        for (let item = testCase.Window.window.activeFocusItem; item; item = item.parent)
            if (item.objectName !== "" && item.objectName !== undefined) return item.objectName
        return ""
    }

    // ---- Names -----------------------------------------------------------------------------------------

    function test_theGearNamesItsGraphAndSaysTheSameInItsTooltip_data() {
        return allKeys.map(key => ({tag: key, key: key}))
    }

    function test_theGearNamesItsGraphAndSaysTheSameInItsTooltip(data) {
        const gear = makeGear(data.key)
        verify(gear.Accessible.name.length > 0)
        verify(gear.Accessible.name.indexOf(gear.graphName) === 0, gear.Accessible.name)
        compare(gear.ToolTip.text, gear.Accessible.name)
    }

    function test_thePopupIsADialogNamedAfterItsGraph_data() {
        return allKeys.map(key => ({tag: key, key: key}))
    }

    function test_thePopupIsADialogNamedAfterItsGraph(data) {
        const gear = makeGear(data.key)
        openPopup(gear)
        const content = gear.popup.contentItem
        compare(content.Accessible.role, Accessible.Dialog)
        verify(content.Accessible.name.indexOf(gear.graphName) >= 0, content.Accessible.name)
    }

    function test_everyControlInThePopupHasAnAccessibleName_data() {
        return [
            {tag: "scenarioHistory", key: "scenarioHistory"},
            {tag: "dailyPlaytime", key: "dailyPlaytime"},
            {tag: "benchmarkRank", key: "benchmarkRank"},
            {tag: "run", key: "run"}
        ]
    }

    function test_everyControlInThePopupHasAnAccessibleName(data) {
        const gear = makeGear(data.key)
        openPopup(gear)
        const names = ["graphField_seriesStyle_connection", "graphField_seriesStyle_markers",
                       "graphMoreSettings_" + data.key, "graphQuickClose_" + data.key]
        if (gear.offersRange) names.push("graphField_dataRange_range")
        if (gear.offersLatestRuns) names.push("graphField_dataRange_latestRuns")
        if (gear.quick.hasWindow === true) names.push("graphField_dataRange_averagingWindow")
        for (const name of names) {
            const item = control(gear, name)
            verify(item !== null, name)
            verify((item.Accessible.name || item.text).length > 0, name + " has no accessible name")
        }
    }

    // ---- Limits and errors ---------------------------------------------------------------------------------

    function test_aFieldStatesItsLimitBeforeAnythingIsWrong() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_averagingWindow")
        verify(field.Accessible.description.replace(/[^0-9]/g, "").indexOf("13650") >= 0, field.Accessible.description)
    }

    function test_anErrorIsTheFieldsDescriptionAndIsTextNotJustColour() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_averagingWindow")
        field.forceActiveFocus()
        field.selectAll()
        keyClick("0")

        const error = control(gear, "graphField_dataRange_averagingWindow_error")
        verify(error.visible)
        verify(error.text.length > 0, "the problem is written out")
        verify(field.Accessible.description.indexOf(error.text) >= 0, "the error is associated with its field")
        compare(error.Accessible.role, Accessible.AlertMessage)
    }

    function test_theLatestRunsFieldAssociatesItsErrorToo() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_latestRuns")
        field.forceActiveFocus()
        keyClick("x")

        const error = control(gear, "graphField_dataRange_latestRuns_error")
        verify(error.visible)
        verify(field.Accessible.description.indexOf(error.text) >= 0)
    }

    function test_theMarkerToggleSaysWhyItIsLockedUnderPointsOnly() {
        fakeVm.setEntry("scenarioHistory", {connection: "pointsOnly"})
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        const markers = control(gear, "graphField_seriesStyle_markers")
        compare(markers.enabled, false)
        verify(markers.Accessible.description.length > 0, "a disabled control explains itself")
    }

    // ---- Keyboard ----------------------------------------------------------------------------------------------

    function test_tabWalksEveryControlOfThePopupInOrder() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        tryVerify(() => focusedName() === "graphField_dataRange_range", 2000)

        const seen = []
        for (let i = 0; i < 8; ++i) {
            keyClick(Qt.Key_Tab)
            seen.push(focusedName())
        }
        const wanted = ["graphField_dataRange_latestRuns", "graphField_seriesStyle_connection",
                        "graphField_seriesStyle_markers", "graphMoreSettings_scenarioHistory"]
        let last = -1
        for (const name of wanted) {
            const at = seen.indexOf(name)
            verify(at > last, name + " was not reached in order, saw " + JSON.stringify(seen))
            last = at
        }
    }

    function test_shiftTabWalksBackToTheFirstControl() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        keyClick(Qt.Key_Tab)
        keyClick(Qt.Key_Tab)
        verify(focusedName() !== "graphField_dataRange_range")

        keyClick(Qt.Key_Tab, Qt.ShiftModifier)
        keyClick(Qt.Key_Tab, Qt.ShiftModifier)
        compare(focusedName(), "graphField_dataRange_range")
    }

    function test_escapeLeavesFromAnyControlAndFocusReturnsToTheGear() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        for (let i = 0; i < 4; ++i) keyClick(Qt.Key_Tab)
        verify(testCase.Window.window.activeFocusItem !== gear)

        keyClick(Qt.Key_Escape)

        tryVerify(() => !gear.popup.opened, 2000)
        tryVerify(() => gear.activeFocus, 2000, "focus returns to the opener")
    }

    function test_tabbingNeverGetsStuckOnOneControl() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const seen = new Set()
        for (let i = 0; i < 12; ++i) {
            keyClick(Qt.Key_Tab)
            seen.add(focusedName())
        }
        verify(seen.size >= 4, "focus moved through " + seen.size + " controls: " + JSON.stringify([...seen]))
    }

    function test_everyControlReachedByTabShowsKeyboardFocus() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        for (let i = 0; i < 5; ++i) {
            keyClick(Qt.Key_Tab)
            const focused = testCase.Window.window.activeFocusItem
            verify(focused !== null && focused.activeFocus)
            // Text inputs are drawn focused by their active focus; every other control reports visualFocus.
            if (focused.visualFocus !== undefined)
                compare(focused.visualFocus, true, focused.objectName + " shows keyboard focus")
        }
    }
}
