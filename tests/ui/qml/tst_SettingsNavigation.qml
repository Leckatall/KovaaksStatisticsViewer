import QtQuick
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "SettingsNavigationTest"
    when: windowShown
    visible: true
    width: 700
    height: 500

    Component { id: dialogComponent; SettingsDialog {} }
    Component { id: vmComponent; FakeQuickSettingsVm {} }

    function makeDialog() {
        const vm = createTemporaryObject(vmComponent, testCase)
        const session = {profileBuildInProgress: false, generateProfile: function () {}}
        const dialog = createTemporaryObject(dialogComponent, testCase, {settingsVm: vm, sessionVm: session})
        dialog.openProfile()
        verify(waitForRendering(dialog.contentItem))
        return {dialog: dialog, vm: vm}
    }

    function find(dialog, name) {
        return ItemLookup.findByObjectName(dialog.contentItem, name) || ItemLookup.findByObjectName(dialog.footer, name)
    }

    function test_sidebarListsGraphsAndSwitchesPages() {
        const {dialog, vm} = makeDialog()
        for (const key of ["profile", "run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"])
            verify(find(dialog, "settingsNav_" + key) !== null, key)
        mouseClick(find(dialog, "settingsNav_dailyPlaytime"))
        compare(dialog.currentPage, "dailyPlaytime")
        compare(vm.selectedGraph, "dailyPlaytime")
        mouseClick(find(dialog, "settingsNav_profile"))
        compare(dialog.currentPage, "profile")
        compare(vm.endVisitCalls, 0)
    }

    function test_sectionsAndRememberedPlace() {
        const {dialog} = makeDialog()
        dialog.openGraph("run")
        tryVerify(() => find(dialog, "settingsNav_run_axes") !== null)
        wait(100)
        mouseClick(find(dialog, "settingsNav_run_axes"))
        compare(dialog.currentSection, "axes")
        dialog.close()
        dialog.open()
        tryCompare(dialog, "currentPage", "run")
        tryCompare(dialog, "currentSection", "axes")
    }

    function test_changedAndIssueMarkers() {
        const {dialog, vm} = makeDialog()
        vm.changedGraphs = ["dailyPlaytime"]
        const entry = find(dialog, "settingsNav_dailyPlaytime")
        tryCompare(entry, "marker", "changed")
        dialog.openGraph("dailyPlaytime")
        vm.draftIssues = [{graph: "dailyPlaytime", group: "dataRange", field: "averagingWindow"}]
        dialog.openProfile()
        tryCompare(entry, "marker", "issue")
        vm.draftIssues = []
        tryCompare(entry, "marker", "changed")
    }

    function test_footerShowsChangedAndRejectedGraphs() {
        const {dialog, vm} = makeDialog()
        vm.changedGraphs = ["dailyPlaytime", "scenarioHistory"]
        const label = find(dialog, "settingsStatusLabel")
        verify(label !== null)
        compare(label.text, "Unsaved changes in Daily playtime, Scenario history.")
        vm.pendingChanges = true
        vm.draftIssues = [{graph: "scenarioHistory", group: "seriesStyle", field: "width"}]
        vm.saveResult = {accepted: false, issues: vm.draftIssues}
        mouseClick(find(dialog, "settingsSaveButton"))
        compare(label.text, "Nothing was saved: fix the marked settings in Scenario history.")
        vm.draftIssues = []
        compare(label.text, "Unsaved changes in Daily playtime, Scenario history.")
    }

    // The store can refuse a Save over something the draft never flagged.
    function test_aRejectionTheDraftDoesNotShareStillSaysNothingWasSaved() {
        const {dialog, vm} = makeDialog()
        vm.pendingChanges = true
        vm.changedGraphs = ["run"]
        vm.saveResult = {accepted: false, issues: [{graph: "run", group: "graphLines", target: "", field: "", code: "invalidRunSeries"}]}
        mouseClick(find(dialog, "settingsSaveButton"))
        const label = find(dialog, "settingsStatusLabel")
        compare(label.text, "Nothing was saved: fix the marked settings in Run graph.")
        compare(find(dialog, "settingsNav_run").marker, "issue")
        vm.issues = []
        compare(label.text, "Unsaved changes in Run graph.")
        compare(find(dialog, "settingsNav_run").marker, "changed")
    }

    function test_rejectedSaveHasNoIssueSummary() {
        const {dialog, vm} = makeDialog()
        dialog.openGraph("dailyPlaytime")
        vm.pendingChanges = true
        vm.draftIssues = [{graph: "dailyPlaytime", group: "dataRange", field: "averagingWindow", code: "nonPositiveCount"}]
        vm.saveResult = {accepted: false, issues: vm.draftIssues}
        mouseClick(find(dialog, "settingsSaveButton"))
        verify(find(dialog, "graphIssueSummary") === null)
    }

    function test_keyboardMovesFromProfileToRunSection() {
        const {dialog} = makeDialog()
        const profile = find(dialog, "settingsNav_profile")
        profile.forceActiveFocus()
        keyClick(Qt.Key_Down)
        tryCompare(dialog, "currentPage", "run")
        tryVerify(() => find(dialog, "settingsNav_run_seriesStyle").activeFocus)
        keyClick(Qt.Key_Down)
        tryCompare(dialog, "currentSection", "axes")
    }

    function test_upFromAGraphsFirstSectionEntersThePreviousGraphAtItsLastSection() {
        const {dialog} = makeDialog()
        dialog.openGraph("dailyPlaytime")
        tryVerify(() => find(dialog, "settingsNav_dailyPlaytime_dataRange") !== null)
        find(dialog, "settingsNav_dailyPlaytime_dataRange").forceActiveFocus()
        keyClick(Qt.Key_Up)
        tryCompare(dialog, "currentPage", "run")
        tryVerify(() => find(dialog, "settingsNav_run_axes") && find(dialog, "settingsNav_run_axes").activeFocus)
        tryCompare(dialog, "currentSection", "axes")
        keyClick(Qt.Key_Up)
        tryVerify(() => find(dialog, "settingsNav_run_seriesStyle").activeFocus)
    }

    // The toolbar's Settings button calls open() on a window that may already be showing.
    function test_openOnAnOpenDialogKeepsItsPage() {
        const {dialog} = makeDialog()
        dialog.close()
        dialog.openGraph("dailyPlaytime")
        dialog.open()
        compare(dialog.currentPage, "dailyPlaytime")
    }

    function test_headingAndGraphEntryPresentation() {
        const {dialog} = makeDialog()
        const heading = find(dialog, "settingsNavHeading")
        compare(heading.text, "GRAPHS")
        verify(heading.font.bold)
        compare(heading.Accessible.role, Accessible.Heading)
        verify(!heading.activeFocusOnTab)
        dialog.openGraph("run")
        const run = find(dialog, "settingsNav_run")
        verify(run.emphasised)
        verify(!run.highlighted)
        verify(find(dialog, "settingsNav_run_accent").visible)
        compare(find(dialog, "settingsNav_run_seriesStyle").text, "Lines & style")
    }

    function test_openGraphLinesResetsTheSelectedGraph() {
        const {dialog, vm} = makeDialog()
        dialog.openGraph("dailyPlaytime")
        dialog.openGraphLines()
        compare(dialog.currentPage, "run")
        compare(vm.selectedGraph, "run")
        tryCompare(dialog, "currentSection", "seriesStyle")
    }

    function test_rejectedMarkersAndStatusFollowLiveIssues() {
        const {dialog, vm} = makeDialog()
        vm.pendingChanges = true
        vm.changedGraphs = ["run", "dailyPlaytime"]
        const runIssue = {graph: "run", group: "graphLines", target: "axis:a1", field: "name", code: "axisNameBlank"}
        const dailyIssue = {graph: "dailyPlaytime", group: "dataRange", field: "averagingWindow", code: "nonPositiveCount"}
        vm.draftIssues = [runIssue, dailyIssue]
        vm.saveResult = {accepted: false, issues: [runIssue]}
        mouseClick(find(dialog, "settingsSaveButton"))
        const label = find(dialog, "settingsStatusLabel")
        compare(label.text, "Nothing was saved: fix the marked settings in Run graph, Daily playtime.")
        compare(find(dialog, "settingsNav_run").marker, "issue")
        vm.issues = []
        vm.draftIssues = [dailyIssue]
        compare(label.text, "Nothing was saved: fix the marked settings in Daily playtime.")
        compare(find(dialog, "settingsNav_run").marker, "changed")
        vm.draftIssues = []
        compare(label.text, "Unsaved changes in Run graph, Daily playtime.")
        vm.draftIssues = [dailyIssue]
        compare(label.text, "Unsaved changes in Run graph, Daily playtime.")
    }

    function test_reopeningGraphStartsAtItsFirstSection() {
        const {dialog} = makeDialog()
        dialog.openGraph("scenarioHistory")
        wait(100)
        mouseClick(find(dialog, "settingsNav_scenarioHistory_axes"))
        compare(dialog.currentSection, "axes")
        dialog.openGraph("scenarioHistory")
        tryCompare(dialog, "currentSection", "dataRange")
    }

    function test_unknownGraphRestoresRememberedPlaceWhileOpen() {
        const {dialog} = makeDialog()
        dialog.openGraph("scenarioHistory")
        dialog.close()
        dialog.openProfile()
        dialog.openGraph("unknown")
        tryCompare(dialog, "currentPage", "scenarioHistory")
    }

    function test_onlyLatestRejectedSaveMarksItsGraphs() {
        const {dialog, vm} = makeDialog()
        const run = {graph: "run", group: "graphLines", field: "name"}
        const daily = {graph: "dailyPlaytime", group: "dataRange", field: "averagingWindow"}
        vm.draftIssues = [run, daily]
        dialog.recordSave({accepted: false, issues: [run]})
        compare(find(dialog, "settingsNav_run").marker, "issue")
        dialog.recordSave({accepted: false, issues: [daily]})
        compare(find(dialog, "settingsNav_run").marker, "")
        compare(find(dialog, "settingsNav_dailyPlaytime").marker, "issue")
    }

    function test_axisIssueFocusesTheFieldInTheRunGraph() {
        const {dialog, vm} = makeDialog()
        const axes = []
        for (let i = 1; i <= 10; ++i)
            axes.push({id: String(i), key: "axis:" + i, name: "Axis " + i, nameText: "Axis " + i,
                       nameIssue: "", lineIds: [], lineNames: [], hasEnabledLine: false, shown: false})
        vm.axisOverview = {shownKey: "", sharedAxes: axes, independentLines: []}
        dialog.openGraph("dailyPlaytime")
        vm.pendingChanges = true
        const issue = {graph: "run", group: "graphLines", target: "axis:10", field: "name", code: "axisNameBlank"}
        vm.draftIssues = [issue]
        vm.saveResult = {accepted: false, issues: [issue]}
        mouseClick(find(dialog, "settingsSaveButton"))
        tryCompare(dialog, "currentPage", "run")
        tryCompare(dialog, "currentSection", "axes")
        tryVerify(() => find(dialog, "axisNameField_10").activeFocus)
        const field = find(dialog, "axisNameField_10")
        const view = find(dialog, "graphEditorScroll")
        tryVerify(() => {
            const pt = field.mapToItem(view, 0, 0)
            return pt.y >= 0 && pt.y + field.height <= view.height
        })
    }
}
