import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "../../../src/ui/qml/graph/GraphCatalog.js" as Catalog
import "ItemLookup.js" as Lookup

TestCase {
    id: testCase
    name: "GraphPagePlan03Test"
    when: windowShown
    width: 520
    height: 430
    visible: true

    Component { id: fakeComponent; FakeQuickSettingsVm {} }
    Component {
        id: editorComponent
        GraphSettingsEditor { width: 500; height: 400 }
    }
    FontMetrics { id: metrics }

    function openEditor(graph) {
        const vm = createTemporaryObject(fakeComponent, testCase)
        const editor = createTemporaryObject(editorComponent, testCase, {settingsVm: vm, graphKey: graph})
        verify(editor !== null)
        verify(waitForRendering(editor))
        return {vm: vm, editor: editor}
    }
    function item(editor, name) {
        const found = Lookup.findByObjectName(editor, name)
        verify(found !== null, name)
        return found
    }
    function issue(graph, group, target, field) {
        return {graph: graph, group: group, target: target, field: field, code: "unparsedInput"}
    }
    function test_issueOrderingFollowsFieldsAndDisplayedRows() {
        const vm = createTemporaryObject(fakeComponent, testCase)
        const pairs = [
            [issue("dailyPlaytime", "dataRange", "", "endDate"), issue("dailyPlaytime", "dataRange", "", "startDate")],
            [issue("scenarioHistory", "axes", "accuracy", "fixedMin"), issue("scenarioHistory", "axes", "score", "fixedMin")],
            [issue("scenarioHistory", "seriesStyle", "accuracy", "width"), issue("scenarioHistory", "seriesStyle", "score", "width")],
            [issue("scenarioHistory", "axes", "score", "fixedMax"), issue("scenarioHistory", "axes", "score", "fixedMin")],
            [issue("scenarioHistory", "axes", "score", "fixedMin"), issue("scenarioHistory", "seriesStyle", "hits", "width")]
        ]
        for (const pair of pairs) {
            compare(Catalog.sortedIssues(pair, vm.graphDrafts, vm.axisOverview)[0], pair[1])
            compare(Catalog.sortedIssues(pair.slice().reverse(), vm.graphDrafts, vm.axisOverview)[0], pair[1])
        }
    }
    function test_issueOrderingToleratesAGraphLinesIssueWithoutATarget() {
        const vm = createTemporaryObject(fakeComponent, testCase)
        const untargeted = {graph: "run", group: "graphLines", field: "name", code: "unparsedInput"}
        const width = issue("run", "seriesStyle", "score", "width")
        compare(Catalog.sortedIssues([width, untargeted], vm.graphDrafts, vm.axisOverview).length, 2)
    }
    // Hidden run-only panels would otherwise rebuild their rows on every edit to another graph.
    function test_runOnlyPanelsAreNotBuiltForOtherGraphs() {
        const {editor} = openEditor("dailyPlaytime")
        compare(Lookup.findByObjectName(editor, "seriesConfigDraftPanel"), null)
        compare(Lookup.findByObjectName(editor, "axesPanel"), null)
        editor.graphKey = "run"
        verify(Lookup.findByObjectName(editor, "seriesConfigDraftPanel") !== null)
        verify(Lookup.findByObjectName(editor, "axesPanel") !== null)
    }
    // Focus reaches a just-opened settings page before its layout has sized the view. Scrolling then
    // would strand the page below its heading row and Reset to defaults.
    function test_ensureVisibleLeavesAnUnsizedViewAlone() {
        const flick = {height: 0, contentY: 0, contentHeight: 669, contentItem: null}
        const field = {height: 24, mapToItem: () => ({y: 51})}
        Catalog.ensureVisible(flick, field, 8)
        compare(flick.contentY, 0)
    }
    function test_sections_data() {
        return [
            {tag: "run", graph: "run", sections: ["seriesStyle", "axes"]},
            {tag: "daily", graph: "dailyPlaytime", sections: ["dataRange", "seriesStyle", "axes"]},
            {tag: "scenario", graph: "scenarioHistory", sections: ["dataRange", "seriesStyle", "axes"]},
            {tag: "rank", graph: "benchmarkRank", sections: ["dataRange", "seriesStyle"]},
            {tag: "recentRank", graph: "benchmarkRecentRank", sections: ["dataRange", "seriesStyle"]},
            {tag: "benchmark", graph: "benchmarkPlaytime", sections: ["dataRange", "seriesStyle", "axes"]}
        ]
    }
    function test_sections(data) {
        const {editor} = openEditor(data.graph)
        compare(editor.sections, data.sections)
        compare(editor.sectionInView, data.sections[0])
    }
    function test_scrollTracksSectionsAndClampsAtTheEnd() {
        const {editor} = openEditor("scenarioHistory")
        const flick = editor.scroller.contentItem
        const lines = item(editor, "graphGroup_seriesStyle")
        const position = lines.mapToItem(flick.contentItem, 0, 0).y
        verify(flick.contentHeight - flick.height >= position - 5)
        editor.scrollToSection("seriesStyle")
        compare(editor.sectionInView, "seriesStyle")
        verify(Math.abs(lines.mapToItem(editor.scroller, 0, 0).y) <= 8)
        editor.scrollToSection("axes")
        compare(flick.contentY, Math.max(0, flick.contentHeight - flick.height))
        compare(editor.sectionInView, "axes")
    }
    function checkCompact(editor, name, hasUnit) {
        const field = item(editor, name)
        metrics.font = field.font
        verify(field.width <= 12 * metrics.height, name + " width " + field.width)
        if (hasUnit) {
            const unit = item(editor, name + "_unit")
            const gap = unit.mapToItem(field, 0, 0).x - field.width
            verify(gap >= 0 && gap <= metrics.height, name + " unit gap " + gap)
        }
    }
    function test_compactFieldsAndCombos() {
        const {editor, vm} = openEditor("dailyPlaytime")
        vm.setGraphRangeKind("dailyPlaytime", "recentDays")
        vm.setAxisMode("dailyPlaytime", "playtime", "fixed")
        verify(waitForRendering(editor))
        for (const name of ["graphField_dataRange_recentDays", "graphField_dataRange_averagingWindow",
                            "graphField_axes_playtime_fixedMin", "graphField_axes_playtime_fixedMax"])
            checkCompact(editor, name, true)
        for (const name of ["graphField_dataRange_range", "graphField_axes_playtime_fitMode", "graphField_seriesStyle_connection"]) {
            const combo = item(editor, name)
            metrics.font = combo.font
            verify(combo.width <= 22 * metrics.height, name)
        }
        vm.setGraphRangeKind("dailyPlaytime", "customDates")
        verify(waitForRendering(editor))
        checkCompact(editor, "graphField_dataRange_startDate", false)
        checkCompact(editor, "graphField_dataRange_endDate", false)
    }
    function test_seriesCardHeaderAlignment() {
        const {editor} = openEditor("scenarioHistory")
        const row = item(editor, "graphSeriesRow_score")
        const swatch = item(editor, "graphField_seriesStyle_score_color")
        const name = item(editor, "graphSeriesName_score")
        const width = item(editor, "graphField_seriesStyle_score_width")
        const header = item(editor, "graphSeriesHeader_width")
        verify(row.height >= 36)
        verify(swatch.mapToItem(editor, 0, 0).x < name.mapToItem(editor, 0, 0).x)
        verify(name.mapToItem(editor, 0, 0).x < width.mapToItem(editor, 0, 0).x)
        verify(header.Accessible.ignored)
        const midpoint = header.mapToItem(editor, header.width / 2, 0).x
        const fieldLeft = width.mapToItem(editor, 0, 0).x
        verify(midpoint >= fieldLeft && midpoint <= fieldLeft + width.width)
    }
}
