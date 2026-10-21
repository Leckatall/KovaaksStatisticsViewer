import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "GraphGearTest"
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

    SignalSpy {
        id: moreSpy
        signalName: "moreSettingsRequested"
    }

    function init() {
        fakeVm.patches = []
        fakeVm.rejectWith = []
        fakeVm.selectedGraph = "run"
        fakeVm.quickSettings = ({
            run: fakeVm.entry({hasRange: false, hasLatestRuns: false, hasWindow: false}),
            dailyPlaytime: fakeVm.entry({hasWindow: true, window: 3}),
            scenarioHistory: fakeVm.entry({hasLatestRuns: true, markers: true}),
            benchmarkRank: fakeVm.entry({}),
            benchmarkPlaytime: fakeVm.entry({hasWindow: true, window: 3})
        })
    }

    readonly property var allKeys: ["run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"]

    function makeGear(key) {
        const gear = createTemporaryObject(gearComponent, testCase, {graphKey: key, settingsVm: fakeVm})
        verify(gear !== null, "GraphGear failed to instantiate")
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

    function shown(gear, name) {
        const item = control(gear, name)
        return item !== null && item.visible
    }

    function choose(combo, text) {
        const names = []
        for (let i = 0; i < combo.count; ++i) names.push(combo.textAt(i))
        const index = names.indexOf(text)
        verify(index >= 0, "no entry '" + text + "' among " + JSON.stringify(names))
        combo.currentIndex = index
        combo.activated(index)
    }

    function typeInto(field, text) {
        field.forceActiveFocus()
        field.selectAll()
        for (const character of text) keyClick(character)
    }

    // One edit event carrying the whole text, as a paste would.
    function enterText(field, text) {
        field.forceActiveFocus()
        field.text = text
        field.textEdited()
    }

    function lastPatch() {
        return fakeVm.patches.length > 0 ? fakeVm.patches[fakeVm.patches.length - 1] : null
    }

    // ---- The gear ----------------------------------------------------------------------------------

    function test_gearIsNamedAfterItsGraph_data() {
        return [
            {tag: "run", key: "run", name: "Run graph"},
            {tag: "dailyPlaytime", key: "dailyPlaytime", name: "Daily playtime"},
            {tag: "scenarioHistory", key: "scenarioHistory", name: "Scenario history"},
            {tag: "benchmarkRank", key: "benchmarkRank", name: "Benchmark rank history"},
            {tag: "benchmarkPlaytime", key: "benchmarkPlaytime", name: "Benchmark playtime"}
        ]
    }

    function test_gearIsNamedAfterItsGraph(data) {
        const gear = makeGear(data.key)
        compare(gear.objectName, "graphGear_" + data.key)
        compare(gear.Accessible.name, data.name + " settings")
        compare(gear.ToolTip.text, data.name + " settings")
        compare(gear.popup.objectName, "graphQuickPopup_" + data.key)
        verify(gear.display === AbstractButton.IconOnly, "the gear is an icon, not an Options text button")
    }

    function test_gearOpensAndClosesThePopup() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        mouseClick(gear)
        tryVerify(() => !gear.popup.opened, 2000)
    }

    function test_gearOpensWithoutAnyViewModelDetail() {
        const gear = createTemporaryObject(gearComponent, testCase, {graphKey: "dailyPlaytime", settingsVm: null})
        mouseClick(gear)
        tryVerify(() => gear.popup.opened, 2000)
        verify(shown(gear, "graphMoreSettings_dailyPlaytime"))
    }

    // ---- Chrome ------------------------------------------------------------------------------------

    function test_theGearIconIsTintedNotBlack() {
        const gear = makeGear("run")
        verify(Qt.colorEqual(gear.icon.color, "#000000") === false, "Lucide strokes use currentColor, which renders black")
        verify(gear.icon.color.a > 0 && gear.icon.color.a < 1, "at rest the icon is set and muted")
    }

    function test_theGearBrightensAndFillsWhileThePopupIsOpen() {
        const gear = makeGear("run")
        mouseMove(testCase, 5, 500)
        const restingAlpha = gear.icon.color.a
        compare(gear.background.color.a, 0)
        openPopup(gear)
        mouseMove(testCase, 5, 500)
        compare(gear.icon.color.a, 1)
        verify(gear.background.color.a > 0, "the open gear keeps a fill")
        mouseClick(gear)
        tryVerify(() => !gear.popup.opened, 2000)
        mouseMove(testCase, 5, 500)
        tryCompare(gear.icon.color, "a", restingAlpha)
        compare(gear.background.color.a, 0)
    }

    function test_theTooltipIsHiddenWhileThePopupIsOpen() {
        const gear = makeGear("run")
        mouseMove(gear, gear.width / 2, gear.height / 2)
        tryVerify(() => gear.pointerHovered, 2000)
        // Opened without a press, since pressing the button hides its tooltip regardless.
        gear.popup.open()
        tryVerify(() => gear.popup.opened, 2000)
        wait(gear.ToolTip.delay + 200)
        verify(gear.pointerHovered)
        verify(!gear.ToolTip.visible)
    }

    function test_thePopupHasABorderedFrame() {
        const gear = makeGear("run")
        const frame = gear.popup.background
        compare(frame.radius, 8)
        compare(frame.border.width, 1)
        verify(Qt.colorEqual(frame.border.color, gear.palette.mid), String(frame.border.color))
        verify(Qt.colorEqual(frame.color, gear.palette.window), String(frame.color))
    }

    function test_theCloseButtonIsAnIcon() {
        const gear = makeGear("run")
        openPopup(gear)
        const close = control(gear, "graphQuickClose_run")
        verify(close !== null)
        compare(close.display, AbstractButton.IconOnly)
        verify(String(close.icon.source).endsWith("x.svg"), String(close.icon.source))
    }

    // ---- Applicable controls only --------------------------------------------------------------------

    function test_popupOffersOnlyTheControlsTheGraphHas_data() {
        return [
            {tag: "run", key: "run", range: false, runs: false, window: false},
            {tag: "dailyPlaytime", key: "dailyPlaytime", range: true, runs: false, window: true},
            {tag: "scenarioHistory", key: "scenarioHistory", range: true, runs: true, window: false},
            {tag: "benchmarkRank", key: "benchmarkRank", range: true, runs: false, window: false},
            {tag: "benchmarkPlaytime", key: "benchmarkPlaytime", range: true, runs: false, window: true}
        ]
    }

    function test_popupOffersOnlyTheControlsTheGraphHas(data) {
        const gear = makeGear(data.key)
        openPopup(gear)

        compare(shown(gear, "graphField_dataRange_range"), data.range, "range presets")
        compare(shown(gear, "graphField_dataRange_latestRuns"), data.runs, "custom latest runs")
        compare(shown(gear, "graphField_dataRange_averagingWindow"), data.window, "averaging window")
        verify(shown(gear, "graphField_seriesStyle_connection"), "connection mode")
        verify(shown(gear, "graphField_seriesStyle_markers"), "marker toggle")
        verify(shown(gear, "graphMoreSettings_" + data.key), "more graph settings")
    }

    function test_rangePresetsDifferForScenarioHistory() {
        const playtime = makeGear("dailyPlaytime")
        openPopup(playtime)
        const playtimeRange = control(playtime, "graphField_dataRange_range")
        const playtimeTexts = []
        for (let i = 0; i < playtimeRange.count; ++i) playtimeTexts.push(playtimeRange.textAt(i))
        compare(playtimeTexts, ["Last 7 days", "Last 30 days", "Last 90 days", "All time"])

        const history = makeGear("scenarioHistory")
        openPopup(history)
        const historyRange = control(history, "graphField_dataRange_range")
        const historyTexts = []
        for (let i = 0; i < historyRange.count; ++i) historyTexts.push(historyRange.textAt(i))
        compare(historyTexts, ["Last 7 days", "Last 30 days", "Last 90 days",
                               "Latest 25 runs", "Latest 50 runs", "Latest 100 runs", "All runs"])
    }

    function test_theMoreSettingsButtonIsTheLastControl() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const more = control(gear, "graphMoreSettings_dailyPlaytime")
        const marker = control(gear, "graphField_seriesStyle_markers")
        verify(more.mapToItem(gear.popup.contentItem, 0, 0).y > marker.mapToItem(gear.popup.contentItem, 0, 0).y)
        compare(more.text, "More graph settings…")
    }

    // ---- Immediate patches -------------------------------------------------------------------------

    function test_choosingARangePresetPatchesImmediately() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)

        choose(control(gear, "graphField_dataRange_range"), "Last 30 days")

        compare(fakeVm.patches.length, 1)
        compare(lastPatch().graph, "dailyPlaytime")
        compare(lastPatch().patch, {rangeKind: "recentDays", count: 30})
        compare(fakeVm.quickSettings.dailyPlaytime.rangeCount, 30)
        compare(control(gear, "graphField_dataRange_range").displayText, "Last 30 days")
    }

    function test_theAllHistoryPresetPatchesAllHistory() {
        fakeVm.setEntry("benchmarkRank", {rangeKind: "recentDays", rangeCount: 7})
        const gear = makeGear("benchmarkRank")
        openPopup(gear)
        compare(control(gear, "graphField_dataRange_range").displayText, "Last 7 days")

        choose(control(gear, "graphField_dataRange_range"), "All time")

        compare(lastPatch().patch, {rangeKind: "allHistory"})
        compare(control(gear, "graphField_dataRange_range").displayText, "All time")
    }

    function test_theLatestRunPresetsPatchLatestRuns() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)

        choose(control(gear, "graphField_dataRange_range"), "Latest 50 runs")

        compare(lastPatch().patch, {rangeKind: "latestRuns", count: 50})
        compare(control(gear, "graphField_dataRange_range").displayText, "Latest 50 runs")
    }

    function test_aCustomLatestRunCountPatchesAsTyped() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)

        typeInto(control(gear, "graphField_dataRange_latestRuns"), "37")

        compare(lastPatch().patch, {rangeKind: "latestRuns", count: "37"})
        compare(fakeVm.quickSettings.scenarioHistory.rangeCount, 37)
        compare(control(gear, "graphField_dataRange_range").displayText, "Latest 37 runs")
    }

    function test_aValidWindowPatchesAsItIsTyped_data() {
        return [{tag: "dailyPlaytime", key: "dailyPlaytime"}, {tag: "benchmarkPlaytime", key: "benchmarkPlaytime"}]
    }

    function test_aValidWindowPatchesAsItIsTyped(data) {
        const gear = makeGear(data.key)
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_averagingWindow")
        compare(field.text, "3")

        typeInto(field, "14")

        compare(lastPatch().patch, {window: "14"})
        compare(fakeVm.quickSettings[data.key].window, 14)
        compare(control(gear, "graphField_dataRange_averagingWindow_error").visible, false)
    }

    function test_changingTheConnectionModePatchesImmediately() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        const connection = control(gear, "graphField_seriesStyle_connection")
        compare(connection.currentText, "Smooth lines")

        choose(connection, "Straight lines")

        compare(lastPatch().patch, {connection: "straight"})
        compare(fakeVm.quickSettings.scenarioHistory.connection, "straight")
        choose(connection, "Points only")
        compare(lastPatch().patch, {connection: "pointsOnly"})
    }

    function test_theMarkerToggleReflectsAndPatchesThePreference() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const markers = control(gear, "graphField_seriesStyle_markers")
        compare(markers.checked, false)
        compare(markers.enabled, true)

        mouseClick(markers)

        compare(lastPatch().patch, {markers: true})
        compare(fakeVm.quickSettings.dailyPlaytime.markers, true)
        compare(markers.checked, true)

        mouseClick(markers)
        compare(lastPatch().patch, {markers: false})
        compare(markers.checked, false)
    }

    function test_pointsOnlyShowsTheMarkerToggleDisabledAndOn() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const markers = control(gear, "graphField_seriesStyle_markers")
        compare(markers.checked, false)

        choose(control(gear, "graphField_seriesStyle_connection"), "Points only")

        compare(markers.enabled, false)
        compare(markers.checked, true)
        compare(fakeVm.quickSettings.dailyPlaytime.markers, false, "the stored line-mode preference is untouched")

        choose(control(gear, "graphField_seriesStyle_connection"), "Straight lines")
        compare(markers.enabled, true)
        compare(markers.checked, false)
    }

    // ---- Truthful display of values only the settings page can set ----------------------------------------

    function test_aCustomDateRangeIsNotShownAsAPreset() {
        fakeVm.setEntry("dailyPlaytime", {rangeKind: "customDates", rangeStart: "2026-08-01", rangeEnd: "2026-08-20",
                                          rangeStartFormatted: "01.08.2026", rangeEndFormatted: "20.08.2026"})
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)

        const range = control(gear, "graphField_dataRange_range")
        compare(range.displayText, "Custom: 01.08.2026 – 20.08.2026", "dates follow the application locale")
        compare(range.currentIndex, -1, "no preset is highlighted for a custom range")
        compare(range.count, 4, "the presets are unchanged")
    }

    function test_aNonPresetCountIsShownAsItsOwnValue() {
        fakeVm.setEntry("scenarioHistory", {rangeKind: "latestRuns", rangeCount: 37})
        fakeVm.setEntry("benchmarkRank", {rangeKind: "recentDays", rangeCount: 45})
        const history = makeGear("scenarioHistory")
        openPopup(history)
        compare(control(history, "graphField_dataRange_range").displayText, "Latest 37 runs")
        compare(control(history, "graphField_dataRange_latestRuns").text, "37")

        const rank = makeGear("benchmarkRank")
        openPopup(rank)
        compare(control(rank, "graphField_dataRange_range").displayText, "Last 45 days")
    }

    function test_countsAreShownInTheApplicationLocale() {
        fakeVm.setEntry("scenarioHistory", {rangeKind: "latestRuns", rangeCount: 37, rangeCountText: "٣٧"})
        fakeVm.setEntry("dailyPlaytime", {window: 14, windowText: "١٤"})
        const history = makeGear("scenarioHistory")
        openPopup(history)
        compare(control(history, "graphField_dataRange_latestRuns").text, "٣٧")
        compare(control(history, "graphField_dataRange_range").displayText, "Latest ٣٧ runs")

        const daily = makeGear("dailyPlaytime")
        openPopup(daily)
        compare(control(daily, "graphField_dataRange_averagingWindow").text, "١٤")
    }

    function test_aPresetMatchingTheStoredValueIsSelected() {
        fakeVm.setEntry("scenarioHistory", {rangeKind: "latestRuns", rangeCount: 100})
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        compare(control(gear, "graphField_dataRange_range").displayText, "Latest 100 runs")
        compare(control(gear, "graphField_dataRange_range").currentIndex, 5)
    }

    function test_theWindowFollowsAChangeCommittedElsewhere() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        fakeVm.setEntry("dailyPlaytime", {window: 9})
        compare(control(gear, "graphField_dataRange_averagingWindow").text, "9")
    }

    // ---- Invalid input stays local ---------------------------------------------------------------------

    function test_invalidWindowTextKeepsTheEffectiveValueAndShowsAnInlineError_data() {
        return [
            {tag: "letters", text: "x"},
            {tag: "fraction", text: "2.5"},
            {tag: "zero", text: "0"},
            {tag: "tooLarge", text: "4000"}
        ]
    }

    function test_invalidWindowTextKeepsTheEffectiveValueAndShowsAnInlineError(data) {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_averagingWindow")
        const error = control(gear, "graphField_dataRange_averagingWindow_error")
        compare(error.visible, false)

        enterText(field, data.text)

        compare(field.text, data.text, "the invalid text stays in the field")
        compare(error.visible, true)
        verify(error.text.length > 0)
        verify(error.text.indexOf("3,650") >= 0 || error.text.indexOf("3650") >= 0, "the error states the representable limit")
        compare(fakeVm.quickSettings.dailyPlaytime.window, 3, "the effective value is intact")
        compare(field.Accessible.description, error.text, "the error is associated with its field")
    }

    function test_theErrorClearsOnceTheTextBecomesValid() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        const field = control(gear, "graphField_dataRange_averagingWindow")
        const error = control(gear, "graphField_dataRange_averagingWindow_error")

        typeInto(field, "x")
        compare(error.visible, true)
        typeInto(field, "5")

        compare(error.visible, false)
        compare(fakeVm.quickSettings.dailyPlaytime.window, 5)
    }

    function test_invalidLatestRunCountShowsItsOwnError() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)

        typeInto(control(gear, "graphField_dataRange_latestRuns"), "0")

        const error = control(gear, "graphField_dataRange_latestRuns_error")
        compare(error.visible, true)
        verify(error.text.indexOf("1,000,000") >= 0 || error.text.indexOf("1000000") >= 0)
        compare(fakeVm.quickSettings.scenarioHistory.rangeKind, "allHistory")
    }

    function test_aRejectedPresetKeepsTheEffectiveValueShown() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        fakeVm.rejectWith = [{graph: "dailyPlaytime", group: "dataRange", target: "", field: "recentDays",
                              code: "countTooLarge"}]

        choose(control(gear, "graphField_dataRange_range"), "Last 7 days")

        compare(control(gear, "graphField_dataRange_range").displayText, "All time")
    }

    function test_reopeningShowsTheEffectiveValueNotTheDiscardedText() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        typeInto(control(gear, "graphField_dataRange_averagingWindow"), "x")
        keyClick(Qt.Key_Escape)
        tryVerify(() => !gear.popup.opened, 2000)

        openPopup(gear)

        compare(control(gear, "graphField_dataRange_averagingWindow").text, "3")
        compare(control(gear, "graphField_dataRange_averagingWindow_error").visible, false)
    }

    // ---- Dismissal and focus ---------------------------------------------------------------------------

    function test_escapeClosesThePopupKeepsAcceptedChangesAndReturnsFocusToTheGear() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        typeInto(control(gear, "graphField_dataRange_averagingWindow"), "12")
        compare(fakeVm.quickSettings.dailyPlaytime.window, 12)

        keyClick(Qt.Key_Escape)

        tryVerify(() => !gear.popup.opened, 2000)
        compare(fakeVm.quickSettings.dailyPlaytime.window, 12, "accepted changes survive dismissal")
        tryVerify(() => gear.activeFocus, 2000, "focus returns to the gear")
    }

    function test_focusMovesIntoThePopupWhenItOpens() {
        const gear = makeGear("dailyPlaytime")
        openPopup(gear)
        verify(gear.popup.contentItem.activeFocus || gear.popup.activeFocus,
               "keyboard focus is inside the popup")
    }

    function test_theCloseButtonDismissesAndReturnsFocus() {
        const gear = makeGear("scenarioHistory")
        openPopup(gear)
        const close = control(gear, "graphQuickClose_scenarioHistory")
        verify(close !== null)
        compare(close.Accessible.name, "Close")

        mouseClick(close)

        tryVerify(() => !gear.popup.opened, 2000)
        tryVerify(() => gear.activeFocus, 2000)
    }

    function test_theGearIsReachableAndActivatableFromTheKeyboard() {
        const gear = makeGear("run")
        gear.forceActiveFocus()
        verify(gear.activeFocus)

        keyClick(Qt.Key_Space)

        tryVerify(() => gear.popup.opened, 2000)
    }

    function test_focusReturnsVisiblyAfterAKeyboardOpenAndClose() {
        const gear = makeGear("run")
        gear.forceActiveFocus(Qt.TabFocusReason)
        verify(gear.visualFocus)
        keyClick(Qt.Key_Space)
        tryVerify(() => gear.popup.opened, 2000)

        keyClick(Qt.Key_Escape)

        tryVerify(() => !gear.popup.opened, 2000)
        tryVerify(() => gear.activeFocus, 2000)
        verify(gear.visualFocus, "a keyboard user must still see where focus landed")
    }

    function test_noFocusRingIsLeftAfterAMouseOpenAndClose() {
        const gear = makeGear("run")
        openPopup(gear)

        mouseClick(gear)

        tryVerify(() => !gear.popup.opened, 2000)
        tryVerify(() => gear.activeFocus, 2000)
        verify(!gear.visualFocus)
    }

    // ---- More graph settings -----------------------------------------------------------------------------

    function test_moreGraphSettingsClosesThePopupAndRequestsTheGraphsPage_data() {
        return allKeys.map(key => ({tag: key, key: key}))
    }

    function test_moreGraphSettingsClosesThePopupAndRequestsTheGraphsPage(data) {
        const gear = makeGear(data.key)
        moreSpy.target = gear
        moreSpy.clear()
        openPopup(gear)

        mouseClick(control(gear, "graphMoreSettings_" + data.key))

        compare(moreSpy.count, 1)
        compare(moreSpy.signalArguments[0][0], data.key)
        tryVerify(() => !gear.popup.opened, 2000)
    }

    function test_quickChangesAreCommittedBeforeNavigating() {
        const gear = makeGear("dailyPlaytime")
        moreSpy.target = gear
        moreSpy.clear()
        openPopup(gear)
        choose(control(gear, "graphField_dataRange_range"), "Last 90 days")
        mouseClick(control(gear, "graphMoreSettings_dailyPlaytime"))

        compare(moreSpy.count, 1)
        compare(fakeVm.quickSettings.dailyPlaytime.rangeKind, "recentDays")
        compare(fakeVm.quickSettings.dailyPlaytime.rangeCount, 90)
    }
}
