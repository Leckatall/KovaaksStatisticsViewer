import QtQuick
import QtQuick.Controls
import QtTest
import QtQuick.Dialogs
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "GraphsSettingsPageTest"
    when: windowShown
    width: 700
    height: 500
    visible: true

    Component {
        id: settingsDialogComponent
        SettingsDialog {}
    }

    Component {
        id: vmComponent
        FakeQuickSettingsVm {}
    }

    function makeSessionVm() {
        return {profileBuildInProgress: false, profileBuildProgress: 0, profileBuildDone: 0, profileBuildTotal: 0,
                generateProfile: function () {}}
    }

    // Opens the dialog on the Graphs page with `graph` selected.
    function openOn(graph) {
        const vm = createTemporaryObject(vmComponent, testCase)
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {settingsVm: vm, sessionVm: makeSessionVm()})
        verify(dialog !== null)
        dialog.openGraph(graph)
        verify(waitForRendering(dialog.contentItem))
        return {dialog: dialog, vm: vm}
    }

    function find(dialog, name) {
        return ItemLookup.findByObjectName(dialog.contentItem, name) || ItemLookup.findByObjectName(dialog.footer, name)
    }

    function shown(dialog, name) {
        const item = find(dialog, name)
        return item !== null && item.visible
    }

    // Scrolls the editor to the control first, as a person would, then clicks it.
    function clickNamed(dialog, name) {
        wait(30)
        const item = find(dialog, name)
        verify(item !== null, name)
        find(dialog, "graphSettingsEditor").ensureVisible(item)
        wait(30)
        mouseClick(item)
    }

    function select(dialog, vm, graph) {
        vm.selectedGraph = graph
        verify(waitForRendering(dialog.contentItem))
    }

    function names(dialog, prefix) {
        return ItemLookup.findByObjectNamePrefix(dialog.contentItem, prefix, [])
            .filter(item => item.visible && !item.objectName.endsWith("_error")).map(item => item.objectName).sort()
    }

    // ---- Control omission (AC01) --------------------------------------------------------------------

    function test_theRunGraphOffersOnlyLineStyleAndTheGraphLinesLink() {
        const {dialog} = openOn("run")

        verify(!shown(dialog, "graphGroup_dataRange"), "the run graph has no date range or averaging")
        verify(!shown(dialog, "graphGroup_axes"), "no axis fitting for the run graph")
        verify(shown(dialog, "graphGroup_seriesStyle"))
        verify(shown(dialog, "graphField_seriesStyle_connection"))
        verify(shown(dialog, "graphField_seriesStyle_markers"))
        verify(shown(dialog, "graphLinesLink"), "individual lines are delegated to Graph Lines")
        compare(names(dialog, "graphField_dataRange_"), [])
        compare(names(dialog, "graphField_axes_"), [])
        compare(names(dialog, "graphSeriesRow_"), [], "no series appearance editor on the run graph")
        verify(shown(dialog, "graphResetButton"))
    }

    function test_dailyPlaytimeOffersRangeWindowOneAxisAndOneSeries() {
        const {dialog} = openOn("dailyPlaytime")

        verify(shown(dialog, "graphGroup_dataRange"))
        verify(shown(dialog, "graphGroup_axes"))
        verify(shown(dialog, "graphGroup_seriesStyle"))
        verify(shown(dialog, "graphField_dataRange_range"))
        verify(shown(dialog, "graphField_dataRange_anchor"))
        verify(shown(dialog, "graphField_dataRange_averagingWindow"))
        verify(!shown(dialog, "graphLinesLink"))
        compare(names(dialog, "graphSeriesRow_"), ["graphSeriesRow_playtime"])
        compare(names(dialog, "graphAxis_"), ["graphAxis_playtime"])
        const range = find(dialog, "graphField_dataRange_range")
        compare(range.entries.map(entry => entry.kind), ["allHistory", "recentDays", "customDates"],
                "latest runs exists for scenario history only")
    }

    function test_scenarioHistoryOffersLatestRunsFiveSeriesAndThreeAxesButNoWindow() {
        const {dialog} = openOn("scenarioHistory")

        verify(!shown(dialog, "graphField_dataRange_averagingWindow"))
        const range = find(dialog, "graphField_dataRange_range")
        compare(range.entries.map(entry => entry.kind), ["allHistory", "recentDays", "latestRuns", "customDates"])
        compare(range.entries[0].text, "All runs")
        compare(names(dialog, "graphSeriesRow_").length, 5)
        compare(names(dialog, "graphAxis_"), ["graphAxis_accuracy", "graphAxis_counts", "graphAxis_score"])
    }

    function test_benchmarkRankHasRangeButNoWindowNoAxesAndNamesItsOnlySeries() {
        const {dialog} = openOn("benchmarkRank")

        verify(shown(dialog, "graphGroup_dataRange"))
        verify(!shown(dialog, "graphGroup_axes"), "the tier axis offers no numeric fitting")
        verify(!shown(dialog, "graphField_dataRange_averagingWindow"))
        compare(names(dialog, "graphSeriesRow_"), ["graphSeriesRow_rank"])
        compare(find(dialog, "graphSeriesName_rank").text, "Personal-best average rank")
        verify(find(dialog, "graphSeriesSelector") === null, "a single series needs no selector")
    }

    function test_benchmarkPlaytimeOffersTheSameControlsAsDailyPlaytime() {
        const {dialog} = openOn("benchmarkPlaytime")

        verify(shown(dialog, "graphField_dataRange_averagingWindow"))
        verify(shown(dialog, "graphGroup_axes"))
        compare(names(dialog, "graphSeriesRow_"), ["graphSeriesRow_playtime"])
    }

    function test_theEditorNamesTheGraphItChanges() {
        const {dialog} = openOn("dailyPlaytime")

        compare(find(dialog, "graphEditingLabel").text, "Changes apply to Daily playtime.")
    }

    function test_noPreviewIsOnThePage() {
        const {dialog} = openOn("scenarioHistory")

        verify(find(dialog, "graphPreview") === null)
        compare(ItemLookup.findByObjectNamePrefix(dialog.contentItem, "graphCanvas", []).length, 0)
    }

    // ---- Data and range ------------------------------------------------------------------------------

    function test_choosingARangeKindAsksTheViewModel() {
        const {dialog, vm} = openOn("scenarioHistory")
        const range = find(dialog, "graphField_dataRange_range")

        range.activated(2)

        compare(vm.callsNamed("setGraphRangeKind").map(c => c.args), [["scenarioHistory", "latestRuns"]])
        tryVerify(() => shown(dialog, "graphField_dataRange_latestRuns"))
        verify(!shown(dialog, "graphField_dataRange_recentDays"))
        verify(!shown(dialog, "graphField_dataRange_startDate"))
        compare(find(dialog, "graphField_dataRange_latestRuns").text, "50")
    }

    function test_theDaysFieldTakesTypedTextVerbatimAndHasPresets() {
        const {dialog, vm} = openOn("dailyPlaytime")
        vm.setGraphRangeKind("dailyPlaytime", "recentDays")
        const field = find(dialog, "graphField_dataRange_recentDays")
        verify(field.visible)
        compare(field.text, "30")

        field.forceActiveFocus()
        field.selectAll()
        keyClick("9")
        keyClick("0")

        const typed = vm.callsNamed("setGraphField").slice(-1)[0].args
        compare(typed, ["dailyPlaytime", "dataRange", "", "recentDays", "90"])
        clickNamed(dialog, "graphRangePreset_recentDays_7")
        compare(vm.callsNamed("setGraphField").slice(-1)[0].args, ["dailyPlaytime", "dataRange", "", "recentDays", "7"])
    }

    function test_scenarioHistoryLatestRunsHasItsOwnPresets() {
        const {dialog, vm} = openOn("scenarioHistory")
        vm.setGraphRangeKind("scenarioHistory", "latestRuns")

        clickNamed(dialog, "graphRangePreset_latestRuns_100")

        compare(vm.callsNamed("setGraphField").slice(-1)[0].args, ["scenarioHistory", "dataRange", "", "latestRuns", "100"])
    }

    function test_customDatesRevealInclusiveFromAndToFieldsInTheLocaleFormat() {
        const {dialog, vm} = openOn("dailyPlaytime")
        vm.setGraphRangeKind("dailyPlaytime", "customDates")
        const start = find(dialog, "graphField_dataRange_startDate")
        const end = find(dialog, "graphField_dataRange_endDate")

        verify(start.visible && end.visible)
        compare(start.text, "17.09.2026")
        compare(end.text, "16.10.2026")
        verify(start.placeholderText.indexOf("dd.MM.yyyy") >= 0, "the entry format is shown")

        end.forceActiveFocus()
        end.selectAll()
        keyClick("5")
        compare(vm.callsNamed("setGraphField").slice(-1)[0].args, ["dailyPlaytime", "dataRange", "", "endDate", "5"],
                "the text goes to the view model unparsed")
    }

    function test_aMovingRangeIsDistinguishedFromFixedDates() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const summary = find(dialog, "graphRangeSummary")
        verify(summary.text.indexOf("all") >= 0)

        vm.setGraphRangeKind("dailyPlaytime", "recentDays")
        verify(summary.text.indexOf("Moving range") === 0, summary.text)
        verify(summary.text.indexOf("today") >= 0)

        vm.setGraphAnchor("dailyPlaytime", true)
        verify(summary.text.indexOf("latest recorded run") >= 0, summary.text)

        vm.setGraphRangeKind("dailyPlaytime", "customDates")
        verify(summary.text.indexOf("Fixed dates") === 0, summary.text)
        verify(summary.text.indexOf("17.09.2026") >= 0 && summary.text.indexOf("16.10.2026") >= 0)
        verify(summary.text.indexOf("inclusive") >= 0)
    }

    function test_theAnchorToggleIsLabelledAndSavedIndependentlyOfTheKind() {
        const {dialog, vm} = openOn("benchmarkRank")
        const anchor = find(dialog, "graphField_dataRange_anchor")

        compare(anchor.text, "End date ranges at latest recorded run")
        compare(anchor.checked, false)
        anchor.toggle()
        anchor.toggled()

        compare(vm.callsNamed("setGraphAnchor").map(c => c.args), [["benchmarkRank", true]])
        compare(anchor.checked, true)
    }

    function test_theAveragingWindowStatesItsLimitAndUnit() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const field = find(dialog, "graphField_dataRange_averagingWindow")

        compare(field.text, "3")
        const hint = find(dialog, "graphField_dataRange_averagingWindow_hint")
        verify(hint.text.indexOf("3,650") >= 0 || hint.text.indexOf("3650") >= 0 || hint.text.indexOf("3.650") >= 0, hint.text)
        field.forceActiveFocus()
        field.selectAll()
        keyClick("7")
        compare(vm.callsNamed("setGraphField").slice(-1)[0].args, ["dailyPlaytime", "dataRange", "", "averagingWindow", "7"])
    }

    // ---- Axes ---------------------------------------------------------------------------------------

    function test_aSharedAxisNamesItsSeriesAndAFixedAxisShowsBoundsInDisplayedUnits() {
        const {dialog, vm} = openOn("scenarioHistory")

        verify(find(dialog, "graphAxisSeries_counts").text.indexOf("Shots, Hits, Misses") >= 0,
               find(dialog, "graphAxisSeries_counts").text)
        verify(!shown(dialog, "graphField_axes_accuracy_fixedMin"))
        find(dialog, "graphField_axes_accuracy_fitMode").activated(2)

        compare(vm.callsNamed("setAxisMode").map(c => c.args), [["scenarioHistory", "accuracy", "fixed"]])
        tryVerify(() => shown(dialog, "graphField_axes_accuracy_fixedMin"))
        compare(find(dialog, "graphField_axes_accuracy_fixedMin_unit").text, "%")
        const min = find(dialog, "graphField_axes_accuracy_fixedMin")
        min.forceActiveFocus()
        min.selectAll()
        keyClick("8")
        compare(vm.callsNamed("setGraphField").slice(-1)[0].args,
                ["scenarioHistory", "axes", "accuracy", "fixedMin", "8"])
    }

    function test_playtimeBoundsAreInMinutes() {
        const {dialog, vm} = openOn("dailyPlaytime")
        vm.setAxisMode("dailyPlaytime", "playtime", "fixed")

        compare(find(dialog, "graphField_axes_playtime_fixedMax_unit").text, "minutes")
        compare(find(dialog, "graphField_axes_playtime_fitMode").currentIndex, 2)
    }

    function test_theFitChoicesAreFitZeroAndFixed() {
        const {dialog} = openOn("dailyPlaytime")

        const mode = find(dialog, "graphField_axes_playtime_fitMode")
        compare(mode.entries.map(entry => entry.text), ["Fit visible data", "Start at zero", "Fixed bounds"])
        compare(mode.currentIndex, 1)
    }

    // ---- Series and style ---------------------------------------------------------------------------

    function test_connectionAndMarkersEditTheDraft() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const connection = find(dialog, "graphField_seriesStyle_connection")
        const markers = find(dialog, "graphField_seriesStyle_markers")

        connection.activated(2)
        compare(vm.callsNamed("setGraphConnection").map(c => c.args), [["dailyPlaytime", "pointsOnly"]])
        tryVerify(() => markers.checked && !markers.enabled, 2000, "Points only shows markers and locks the toggle")

        connection.activated(0)
        tryVerify(() => markers.enabled && !markers.checked)
        markers.toggle()
        markers.toggled()
        compare(vm.callsNamed("setGraphMarkers").map(c => c.args), [["dailyPlaytime", true]])
    }

    function test_theColourSwatchOpensTheColourDialogAndAcceptingStagesTheColour() {
        const {dialog, vm} = openOn("scenarioHistory")
        clickNamed(dialog, "graphField_seriesStyle_hits_color")
        const picker = find(dialog, "graphSettingsEditor").colorDialog
        compare(picker.selectedColor.toString(), "#7cb342")
        picker.selectedColor = "#123456"
        picker.accepted()

        compare(vm.callsNamed("setSeriesColor").map(c => c.args), [["scenarioHistory", "hits", "#123456"]])
        wait(30)
        compare(find(dialog, "graphField_seriesStyle_hits_color").swatchColor.toString(), "#123456")
    }

    function test_aSeriesWidthIsTypedPerSeriesAndStatesItsLimits() {
        const {dialog, vm} = openOn("scenarioHistory")
        const width = find(dialog, "graphField_seriesStyle_accuracy_width")

        compare(width.text, "3")
        width.forceActiveFocus()
        width.selectAll()
        keyClick("5")
        compare(vm.callsNamed("setGraphField").slice(-1)[0].args,
                ["scenarioHistory", "seriesStyle", "accuracy", "width", "5"])
        verify(find(dialog, "graphSeriesWidthHint").text.indexOf("12") >= 0)
    }

    function test_theSeriesDefaultButtonClearsBothOverrides() {
        const {dialog, vm} = openOn("scenarioHistory")
        vm.setSeriesColor("scenarioHistory", "shots", "#111111")

        clickNamed(dialog, "graphSeriesReset_shots")

        compare(vm.callsNamed("clearSeriesOverride").map(c => c.args),
                [["scenarioHistory", "shots", "color"], ["scenarioHistory", "shots", "width"]])
    }

    function test_theRankSeriesWithoutAColourOverrideShowsAThemeSwatch() {
        const {dialog} = openOn("benchmarkRank")

        const swatch = find(dialog, "graphField_seriesStyle_rank_color")
        compare(swatch.usesThemeColor, true)
    }

    function test_theGraphLinesLinkOpensGraphLinesWithoutEndingTheVisit() {
        const {dialog, vm} = openOn("run")
        const visits = vm.beginVisitCalls

        clickNamed(dialog, "graphLinesLink")

        compare(dialog.currentCategory, dialog.graphLinesCategory)
        compare(vm.endVisitCalls, 0)
        compare(vm.beginVisitCalls, visits)
        compare(vm.selectedGraph, "run")
    }

    // ---- Errors --------------------------------------------------------------------------------------

    function issue(graph, group, target, field, code) {
        return {graph: graph, group: group, target: target, field: field, code: code}
    }

    function test_anInlineErrorSitsNextToTheControlAndNamesTheLimit() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const error = find(dialog, "graphField_dataRange_averagingWindow_error")
        verify(!error.visible)

        vm.draftIssues = [issue("dailyPlaytime", "dataRange", "", "averagingWindow", "nonPositiveCount")]

        verify(error.visible)
        verify(error.text.indexOf("whole number") >= 0 && error.text.indexOf("1") >= 0, error.text)
        verify(find(dialog, "graphField_dataRange_averagingWindow").Accessible.description.indexOf(error.text) >= 0,
               "the error is associated with the control")
    }

    function test_theErrorsOfOtherGraphsDoNotShowOnTheSelectedOne() {
        const {dialog, vm} = openOn("benchmarkPlaytime")

        vm.draftIssues = [issue("dailyPlaytime", "dataRange", "", "averagingWindow", "nonPositiveCount")]

        verify(!find(dialog, "graphField_dataRange_averagingWindow_error").visible)
    }

    function test_dateWidthAndBoundErrorsSayWhatIsWrong() {
        const {dialog, vm} = openOn("scenarioHistory")
        vm.setGraphRangeKind("scenarioHistory", "customDates")
        vm.setAxisMode("scenarioHistory", "score", "fixed")

        vm.draftIssues = [
            issue("scenarioHistory", "dataRange", "", "endDate", "reversedDateRange"),
            issue("scenarioHistory", "dataRange", "", "startDate", "unparsedInput"),
            issue("scenarioHistory", "seriesStyle", "hits", "width", "lineWidthOutOfRange"),
            issue("scenarioHistory", "axes", "score", "fixedMax", "fixedBoundsNotIncreasing"),
            issue("scenarioHistory", "axes", "score", "fixedMin", "unparsedInput")
        ]

        verify(find(dialog, "graphField_dataRange_endDate_error").text.indexOf("end date") >= 0)
        verify(find(dialog, "graphField_dataRange_startDate_error").text.indexOf("dd.MM.yyyy") >= 0)
        verify(find(dialog, "graphField_seriesStyle_hits_width_error").text.indexOf("0.5") >= 0)
        verify(find(dialog, "graphField_axes_score_fixedMax_error").text.indexOf("greater") >= 0)
        verify(find(dialog, "graphField_axes_score_fixedMin_error").text.indexOf("number") >= 0)
    }

    // ---- The draft across navigation -----------------------------------------------------------------

    function test_aTypedValueAndItsErrorSurviveGraphAndCategorySwitches() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const field = find(dialog, "graphField_dataRange_averagingWindow")
        field.forceActiveFocus()
        field.selectAll()
        keyClick("x")
        vm.draftIssues = [issue("dailyPlaytime", "dataRange", "", "averagingWindow", "unparsedInput")]

        select(dialog, vm, "scenarioHistory")
        mouseClick(find(dialog, "categoryItem_Graph Lines"))
        mouseClick(find(dialog, "categoryItem_Graphs"))
        select(dialog, vm, "dailyPlaytime")

        compare(find(dialog, "graphField_dataRange_averagingWindow").text, "x")
        verify(find(dialog, "graphField_dataRange_averagingWindow_error").visible)
        compare(vm.endVisitCalls, 0)
        compare(vm.beginVisitCalls, 1)
    }

    function test_editsToTwoGraphsAreBothStillShownAfterSwitchingBetweenThem() {
        const {dialog, vm} = openOn("dailyPlaytime")
        find(dialog, "graphField_seriesStyle_connection").activated(1)
        select(dialog, vm, "scenarioHistory")
        find(dialog, "graphField_seriesStyle_connection").activated(2)

        select(dialog, vm, "dailyPlaytime")
        compare(find(dialog, "graphField_seriesStyle_connection").currentIndex, 1)
        select(dialog, vm, "scenarioHistory")
        compare(find(dialog, "graphField_seriesStyle_connection").currentIndex, 2)
        compare(vm.saveCalls, 0)
    }

    // ---- Save with issues ----------------------------------------------------------------------------

    function test_aRejectedSaveNamesTheGraphGroupAndControlAndFocusesTheFirstIssue() {
        const {dialog, vm} = openOn("scenarioHistory")
        const problems = [
            issue("dailyPlaytime", "dataRange", "", "averagingWindow", "nonPositiveCount"),
            issue("scenarioHistory", "seriesStyle", "hits", "width", "lineWidthOutOfRange")
        ]
        vm.pendingChanges = true
        vm.draftIssues = problems
        vm.saveResult = {accepted: false, issues: problems}

        mouseClick(find(dialog, "settingsSaveButton"))

        compare(vm.saveCalls, 1)
        const summary = find(dialog, "graphIssueSummary")
        tryVerify(() => summary.visible)
        const lines = ItemLookup.findByObjectNamePrefix(dialog.contentItem, "graphIssueLink_", [])
            .filter(item => item.visible).map(item => item.text)
        compare(lines.length, 2)
        verify(lines[0].indexOf("Daily playtime › Data & range › Rolling average") === 0, lines[0])
        verify(lines[1].indexOf("Scenario history › Series & style › Hits line width") === 0, lines[1])
        compare(vm.selectedGraph, "dailyPlaytime", "focus goes to the first issue, switching graph")
        tryVerify(() => find(dialog, "graphField_dataRange_averagingWindow").activeFocus, 2000)
        compare(dialog.visible, true)
    }

    function test_activatingAnIssueLinkJumpsToItsControl() {
        const {dialog, vm} = openOn("scenarioHistory")
        const problems = [
            issue("dailyPlaytime", "dataRange", "", "averagingWindow", "nonPositiveCount"),
            issue("scenarioHistory", "seriesStyle", "hits", "width", "lineWidthOutOfRange")
        ]
        vm.pendingChanges = true
        vm.draftIssues = problems
        vm.saveResult = {accepted: false, issues: problems}
        mouseClick(find(dialog, "settingsSaveButton"))
        tryVerify(() => vm.selectedGraph === "dailyPlaytime")

        clickNamed(dialog, "graphIssueLink_1")

        compare(vm.selectedGraph, "scenarioHistory")
        tryVerify(() => find(dialog, "graphField_seriesStyle_hits_width").activeFocus, 2000)
    }

    function test_theSummaryDisappearsOnceTheIssuesAreFixed() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const problems = [issue("dailyPlaytime", "dataRange", "", "averagingWindow", "nonPositiveCount")]
        vm.pendingChanges = true
        vm.draftIssues = problems
        vm.saveResult = {accepted: false, issues: problems}
        mouseClick(find(dialog, "settingsSaveButton"))
        tryVerify(() => find(dialog, "graphIssueSummary").visible)

        vm.draftIssues = []

        tryVerify(() => !find(dialog, "graphIssueSummary").visible)
    }

    function test_aGraphLinesIssueOpensGraphLinesInsteadOfTheGraphsPage() {
        const {dialog, vm} = openOn("dailyPlaytime")
        const problems = [issue("run", "graphLines", "records[0].yAxisId", "", "unknownAxis")]
        vm.pendingChanges = true
        vm.draftIssues = problems
        vm.saveResult = {accepted: false, issues: problems}

        mouseClick(find(dialog, "settingsSaveButton"))

        tryCompare(dialog, "currentCategory", dialog.graphLinesCategory)
    }

    // ---- Reset ---------------------------------------------------------------------------------------

    function test_resetStagesOnlyTheSelectedGraph() {
        const {dialog, vm} = openOn("scenarioHistory")

        clickNamed(dialog, "graphResetButton")

        compare(vm.callsNamed("resetGraph").map(c => c.args), [["scenarioHistory"]])
        compare(vm.saveCalls, 0)
        compare(vm.endVisitCalls, 0)
        compare(find(dialog, "graphResetButton").text, "Reset this graph")
        verify(find(dialog, "graphResetNote").text.indexOf("Graph Lines") >= 0, "the note says Graph Lines are untouched")
    }

    // ---- Cancel, discard, cancel-close ---------------------------------------------------------------

    function test_cancelDropsTheDraftAndClosesWithoutAPrompt() {
        const {dialog, vm} = openOn("dailyPlaytime")
        find(dialog, "graphField_seriesStyle_markers").toggled()

        mouseClick(find(dialog, "settingsCancelButton"))

        tryCompare(dialog, "visible", false)
        compare(dialog.discardChangesPrompt.visible, false)
        verify(vm.endVisitCalls >= 1)
        compare(vm.saveCalls, 0)
    }

    function test_closingWithGraphEditsPromptsAndDiscardingDropsThemWithoutSaving() {
        const {dialog, vm} = openOn("dailyPlaytime")
        find(dialog, "graphField_seriesStyle_markers").toggled()
        verify(vm.pendingChanges)

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)

        tryCompare(dialog, "visible", false)
        compare(vm.saveCalls, 0)
        verify(vm.endVisitCalls >= 1)
    }

    function test_cancellingTheCloseKeepsTheWindowAndEveryEdit() {
        const {dialog, vm} = openOn("dailyPlaytime")
        find(dialog, "graphField_seriesStyle_connection").activated(2)
        select(dialog, vm, "scenarioHistory")
        find(dialog, "graphField_seriesStyle_connection").activated(1)

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)

        compare(dialog.visible, true)
        compare(vm.endVisitCalls, 0)
        compare(vm.saveCalls, 0)
        compare(find(dialog, "graphField_seriesStyle_connection").currentIndex, 1)
        select(dialog, vm, "dailyPlaytime")
        compare(find(dialog, "graphField_seriesStyle_connection").currentIndex, 2)
    }

    function test_saveFromThePromptWithAnInvalidFieldKeepsTheWindowAndFocusesTheIssue() {
        const {dialog, vm} = openOn("scenarioHistory")
        const problems = [issue("dailyPlaytime", "dataRange", "", "averagingWindow", "unparsedInput")]
        vm.pendingChanges = true
        vm.draftIssues = problems
        vm.saveResult = {accepted: false, issues: problems}

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)
        dialog.discardChangesPrompt.close()
        dialog.requestActivate()

        compare(dialog.visible, true)
        tryCompare(vm, "selectedGraph", "dailyPlaytime")
        tryVerify(() => find(dialog, "graphField_dataRange_averagingWindow").activeFocus, 2000)
    }

    // ---- Settings size floor and text scale (UI-07) --------------------------------------------------

    function inside(dialog, item) {
        const origin = item.mapToItem(dialog.contentItem, 0, 0)
        return origin.y >= 0 && origin.y + item.height <= dialog.contentItem.height + 1
            && origin.x >= 0 && origin.x + item.width <= dialog.contentItem.width + 1
    }

    function test_atTheSettingsFloorTheSelectorAndFooterStayReachableAndTheEditorScrolls() {
        const {dialog, vm} = openOn("scenarioHistory")
        dialog.width = dialog.floorWidth
        dialog.height = dialog.floorHeight
        verify(waitForRendering(dialog.contentItem))

        verify(inside(dialog, find(dialog, "graphSelector")), "the graph selector is on screen")
        for (const name of ["settingsSaveButton", "settingsCancelButton"]) {
            const button = find(dialog, name)
            verify(button.visible, name)
            const bottom = button.mapToItem(null, 0, button.height).y
            verify(bottom <= dialog.height + 1, name + " ends at " + bottom + " in a window " + dialog.height + " high")
        }
        const scroller = find(dialog, "graphEditorScroll")
        verify(scroller.contentHeight > scroller.height, "more content than room: it scrolls")

        const reset = find(dialog, "graphResetButton")
        const flick = scroller.contentItem
        flick.contentY = Math.max(0, flick.contentHeight - flick.height)
        verify(waitForRendering(dialog.contentItem))
        const resetAt = reset.mapToItem(scroller, 0, 0).y
        verify(resetAt >= 0 && resetAt + reset.height <= scroller.height + 1,
               "the last control is reachable by scrolling (at " + resetAt + " of " + scroller.height + ")")
    }

    function test_theScrollBarNeverCoversAControlOrAUnit_data() {
        const rows = []
        for (const graph of ["run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"]) {
            rows.push({tag: graph + " at the default size", graph: graph, floor: false})
            rows.push({tag: graph + " at the floor", graph: graph, floor: true})
        }
        return rows
    }

    function test_theScrollBarNeverCoversAControlOrAUnit(data) {
        const {dialog} = openOn(data.graph)
        if (data.floor) {
            dialog.width = dialog.floorWidth
            dialog.height = dialog.floorHeight
        }
        verify(waitForRendering(dialog.contentItem))

        const scroller = find(dialog, "graphEditorScroll")
        const bar = scroller.ScrollBar.vertical
        if (scroller.contentHeight <= scroller.height + 1)
            return
        verify(bar.visible, "an overflowing editor shows its scroll bar")
        const barLeft = bar.mapToItem(scroller, 0, 0).x
        const guarded = ItemLookup.findByObjectNamePrefix(find(dialog, "graphSettingsContent"), "graph", [])
            .filter(item => item.visible && item.width > 0 && !item.objectName.endsWith("_error")
                    && (item.objectName.startsWith("graphField_") || item.objectName.endsWith("_unit")))
        verify(guarded.length > 0)
        for (const item of guarded) {
            const right = item.mapToItem(scroller, item.width, 0).x
            verify(right <= barLeft + 0.5,
                   item.objectName + " ends at " + right + " under the scroll bar starting at " + barLeft)
        }
    }

    function test_atALargerTextScaleNothingIsClippedHorizontallyAndTheEditorStillScrolls() {
        const {dialog} = openOn("dailyPlaytime")
        dialog.width = dialog.floorWidth
        dialog.height = dialog.floorHeight
        const font = dialog.font
        font.pixelSize = Math.round(font.pixelSize > 0 ? font.pixelSize * 2 : 28)
        dialog.font = font
        verify(waitForRendering(dialog.contentItem))

        const scroller = find(dialog, "graphEditorScroll")
        verify(scroller.contentHeight > scroller.height)
        verify(find(dialog, "graphSettingsContent").width <= scroller.availableWidth + 1,
               "the editor reflows to the available width")
        for (const name of ["graphField_dataRange_averagingWindow", "graphField_seriesStyle_connection"]) {
            const item = find(dialog, name)
            const right = item.mapToItem(scroller, item.width, 0).x
            verify(right <= scroller.width + 1, name + " ends at " + right + " beyond " + scroller.width)
        }
        verify(find(dialog, "settingsSaveButton").visible)
    }

    // ---- Keyboard and accessible operation (QUAL-01) -------------------------------------------------

    function test_everyControlHasAnAccessibleNameAndTheGroupsAreHeadings() {
        const {dialog} = openOn("scenarioHistory")

        for (const name of ["graphField_dataRange_range", "graphField_dataRange_anchor", "graphField_seriesStyle_connection",
                            "graphField_seriesStyle_markers", "graphField_axes_score_fitMode",
                            "graphField_seriesStyle_hits_color", "graphField_seriesStyle_hits_width", "graphResetButton"]) {
            const item = find(dialog, name)
            verify(item !== null, name)
            verify(item.Accessible.name.length > 0, name + " has no accessible name")
        }
        compare(find(dialog, "graphGroup_dataRange").Accessible.role, Accessible.Heading)
        verify(find(dialog, "graphField_seriesStyle_hits_color").Accessible.name.indexOf("Hits") >= 0)
    }
    // The contextual link hands the keyboard to the editor of the graph it opened, not to the selector.
    function test_openingAGraphFocusesTheFirstControlOfItsEditor_data() {
        return [
            {tag: "dailyPlaytime", graph: "dailyPlaytime", first: "graphField_dataRange_range"},
            {tag: "scenarioHistory", graph: "scenarioHistory", first: "graphField_dataRange_range"},
            {tag: "benchmarkRank", graph: "benchmarkRank", first: "graphField_dataRange_range"},
            {tag: "run", graph: "run", first: "graphField_seriesStyle_connection"}
        ]
    }

    function test_openingAGraphFocusesTheFirstControlOfItsEditor(data) {
        const {dialog} = openOn(data.graph)

        tryVerify(() => find(dialog, data.first) !== null && find(dialog, data.first).activeFocus, 2000,
                  data.first + " should hold keyboard focus")
        verify(!find(dialog, "graphSelector").activeFocus)
    }

    // Walks the window's Tab order from `start` and returns the objectNames it passes before returning.
    function tabOrderFrom(start) {
        const visited = []
        let item = start
        for (let i = 0; i < 400; ++i) {
            item = item.nextItemInFocusChain(true)
            if (item === start) return {names: visited, cycles: true}
            visited.push(item.objectName)
        }
        return {names: visited, cycles: false}
    }

    function test_tabOrderLeadsFromTheEditorToSaveAndCancelAndAroundAgain() {
        const {dialog, vm} = openOn("scenarioHistory")
        vm.pendingChanges = true
        const first = find(dialog, "graphField_dataRange_range")

        const walk = tabOrderFrom(first)

        verify(walk.cycles, "the Tab order comes back to where it started: no trap")
        for (const name of ["graphField_dataRange_latestRuns", "graphField_seriesStyle_connection", "graphField_seriesStyle_markers",
                            "graphField_seriesStyle_hits_color", "graphField_seriesStyle_hits_width",
                            "graphResetButton", "settingsSaveButton", "settingsCancelButton"]) {
            const quiet = name === "graphField_dataRange_latestRuns" && find(dialog, name) && !find(dialog, name).visible
            if (!quiet) verify(walk.names.indexOf(name) >= 0, name + " is not reachable by Tab; saw " + JSON.stringify(walk.names))
        }
        verify(walk.names.indexOf("graphSelector") >= 0, "the graph selector is reachable by Tab")
    }

    function test_theEditorsControlsComeInTheOrderTheyAreRead() {
        const {dialog} = openOn("dailyPlaytime")
        const walk = tabOrderFrom(find(dialog, "graphField_dataRange_range"))
        const order = ["graphField_dataRange_range", "graphField_dataRange_averagingWindow", "graphField_dataRange_anchor",
                       "graphField_axes_playtime_fitMode", "graphField_seriesStyle_connection", "graphResetButton"]
        const names = ["graphField_dataRange_range"].concat(walk.names)
        let last = -1
        for (const name of order) {
            const at = names.indexOf(name)
            verify(at > last, name + " out of order in " + JSON.stringify(names))
            last = at
        }
    }

    function test_everyFieldErrorIsAssociatedWithItsControl() {
        const {dialog, vm} = openOn("scenarioHistory")
        vm.setGraphRangeKind("scenarioHistory", "customDates")
        vm.setAxisMode("scenarioHistory", "score", "fixed")
        vm.draftIssues = [
            issue("scenarioHistory", "dataRange", "", "endDate", "reversedDateRange"),
            issue("scenarioHistory", "seriesStyle", "hits", "width", "lineWidthOutOfRange"),
            issue("scenarioHistory", "axes", "score", "fixedMax", "fixedBoundsNotIncreasing")
        ]

        for (const name of ["graphField_dataRange_endDate", "graphField_seriesStyle_hits_width", "graphField_axes_score_fixedMax"]) {
            const error = find(dialog, name + "_error")
            verify(error.visible && error.text.length > 0, name + " shows its problem as text")
            verify(find(dialog, name).Accessible.description.indexOf(error.text) >= 0, name + " carries its error")
        }
    }

    function test_theLimitsOfAFieldAreItsDescriptionBeforeAnythingIsWrong() {
        const {dialog} = openOn("dailyPlaytime")
        const description = find(dialog, "graphField_dataRange_averagingWindow").Accessible.description
        verify(description.indexOf("3,650") >= 0 || description.indexOf("3650") >= 0 || description.indexOf("3.650") >= 0, description)
    }

    function test_theSelectedPresetIsMarkedBeyondColour() {
        const {dialog, vm} = openOn("dailyPlaytime")
        vm.setGraphRangeKind("dailyPlaytime", "recentDays")
        vm.change("dailyPlaytime", {rangeCount: 30})
        verify(waitForRendering(dialog.contentItem))

        const selected = find(dialog, "graphRangePreset_recentDays_30")
        const other = find(dialog, "graphRangePreset_recentDays_7")
        verify(selected !== null && other !== null)
        compare(selected.font.bold, true)
        compare(other.font.bold, false)
        verify(selected.Accessible.description.length > 0)
        compare(other.Accessible.description, "")
    }

    function test_aColourSwatchShowsKeyboardFocusWithAThickerBorderAndNamesItsSeries() {
        const {dialog} = openOn("scenarioHistory")
        const swatch = find(dialog, "graphField_seriesStyle_hits_color")
        compare(swatch.background.border.width, 1)

        swatch.forceActiveFocus(Qt.TabFocusReason)

        compare(swatch.visualFocus, true)
        compare(swatch.background.border.width, 2)
        verify(swatch.Accessible.name.indexOf("Hits") >= 0)
    }

    function test_theEditorNamesTheGraphItIsEditingAsText() {
        const {dialog} = openOn("benchmarkRank")
        verify(find(dialog, "graphEditingLabel").text.indexOf("Benchmark rank history") >= 0)
    }
}
