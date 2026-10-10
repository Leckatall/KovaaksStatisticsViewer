import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "RunListViewTest"
    when: windowShown
    width: 700
    height: 500
    visible: true

    Component {
        id: viewComponent
        RunListView {}
    }

    SignalSpy {
        id: runSelectedSpy
        signalName: "runSelected"
    }

    SignalSpy {
        id: sortRequestedSpy
        signalName: "sortRequested"
    }

    function makeRuns() {
        return [{hash: "scenario-a", scenarioName: "Scenario A", runLabel: "run", startTimeMs: 1723200000000,
                 score: 8421, accuracy: 0.91, shots: 132, hits: 120, personalBest: true}]
    }

    function makeSortableRuns() {
        return [
            {hash: "a", scenarioName: "Scenario a", runLabel: "run-a", startTimeMs: 3000, score: 8000, accuracy: 0.80, shots: 100, hits: 80, personalBest: false},
            {hash: "b", scenarioName: "Scenario b", runLabel: "run-b", startTimeMs: 1000, score: 9500, accuracy: 0.95, shots: 100, hits: 95, personalBest: false},
            {hash: "c", scenarioName: "Scenario c", runLabel: "run-c", startTimeMs: 2000, score: 7000, accuracy: 0.70, shots: 100, hits: 70, personalBest: false},
        ]
    }

    function createView(props) {
        const view = createTemporaryObject(viewComponent, testCase,
            Object.assign({width: 650, height: 420}, props))
        verify(waitForRendering(view))
        return view
    }

    function test_delegateFormatsAndSelectsRun() {
        const view = createView({runModel: makeRuns()})
        const delegate = findChild(view, "runItem_0")
        verify(delegate !== null)
        verify(findChild(delegate, "runScore_0").text.indexOf("8421") !== -1)
        verify(findChild(delegate, "runAccuracy_0").text.indexOf("91.0%") !== -1)
        verify(findChild(delegate, "runDate_0").text.length > 0)
        verify(findChild(delegate, "personalBestBadge_0").visible)

        runSelectedSpy.target = view
        verify(runSelectedSpy.valid)
        mouseClick(delegate, delegate.width / 2, delegate.height / 2)
        wait(0)
        compare(runSelectedSpy.count, 1)
        compare(runSelectedSpy.signalArguments[0][0], "scenario-a")
        compare(runSelectedSpy.signalArguments[0][1], 1723200000000)
    }

    function test_personalBestBadgeIsHiddenByDefault() {
        const view = createView({runModel: makeSortableRuns()})
        const badge = findChild(view, "personalBestBadge_0")

        verify(badge !== null)
        verify(!badge.visible)
    }

    function test_emptyModelShowsEmptyLabel() {
        const view = createView({runModel: null})
        verify(view.hasNoRuns())
        verify(findChild(view, "runListEmptyLabel").visible)
    }

    function test_sortFieldSelectionEmitsSortRequestedAndReordersRows() {
        const view = createView({runModel: makeSortableRuns()})
        compare(findChild(view, "runScore_0").text, "8000")

        const combo = findChild(view, "runSortCombo")
        verify(combo !== null)
        combo.forceActiveFocus()

        // Material's ComboBox can emit `activated` more than once for a single
        // keyboard selection; track calls directly instead of via SignalSpy so
        // multiple identical emissions don't break argument indexing.
        const captured = []
        const onSort = (field, ascending) => captured.push([field, ascending])
        view.sortRequested.connect(onSort)

        keyClick(Qt.Key_Down)
        wait(50)
        view.sortRequested.disconnect(onSort)

        verify(captured.length >= 1)
        const lastArgs = captured[captured.length - 1]
        compare(lastArgs[0], 1)
        compare(lastArgs[1], false)

        // Simulate what the real VM does on sortRequested: re-sort by score desc.
        view.runModel = [
            {hash: "b", scenarioName: "Scenario b", runLabel: "run-b", startTimeMs: 1000, score: 9500, accuracy: 0.95, shots: 100, hits: 95, personalBest: false},
            {hash: "a", scenarioName: "Scenario a", runLabel: "run-a", startTimeMs: 3000, score: 8000, accuracy: 0.80, shots: 100, hits: 80, personalBest: false},
            {hash: "c", scenarioName: "Scenario c", runLabel: "run-c", startTimeMs: 2000, score: 7000, accuracy: 0.70, shots: 100, hits: 70, personalBest: false},
        ]
        wait(0)

        compare(findChild(view, "runScore_0").text, "9500")
        compare(findChild(view, "runScore_1").text, "8000")
        compare(findChild(view, "runScore_2").text, "7000")
    }

    function test_sortDirectionToggleEmitsSortRequestedWithFlippedDirection() {
        const view = createView({runModel: makeSortableRuns()})
        const dirButton = findChild(view, "runSortDirectionButton")
        verify(dirButton !== null)

        sortRequestedSpy.target = view
        verify(sortRequestedSpy.valid)

        mouseClick(dirButton, dirButton.width / 2, dirButton.height / 2)
        wait(0)

        compare(sortRequestedSpy.count, 1)
        compare(sortRequestedSpy.signalArguments[0][0], 0)
        compare(sortRequestedSpy.signalArguments[0][1], true)
    }
    // --- dense, scannable rows -----------------------------------------

    function makeManyRuns(count) {
        const runs = []
        for (let i = 0; i < count; ++i)
            runs.push({hash: "p", scenarioName: "Pasu Gallery", runLabel: "Pasu Gallery #" + i,
                       startTimeMs: 1723200000000 - i * 3600000, score: 3400 - i * 10,
                       accuracy: 0.68, shots: 100, hits: 68, personalBest: i % 3 === 0})
        return runs
    }

    function test_rowsAreAtLeastTwiceAsDenseAsTheOldCards() {
        const view = createView({runModel: makeManyRuns(12), height: 300})
        const row = findChild(view, "runItem_0")
        verify(row !== null)
        // The old two-line cards took ~60px plus 6px spacing per run.
        verify(row.height <= 33, "a run row is " + row.height + "px; it must be at most half the old 66px pitch")
        verify(findChild(view, "runItem_1").y - row.y <= 33)
    }

    function test_eachRowShowsDateScoreAccuracyAndPersonalBestInColumns() {
        const view = createView({runModel: makeManyRuns(3)})
        compare(Qt.formatDateTime(new Date(1723200000000), "yyyy-MM-dd  HH:mm"), findChild(view, "runDate_0").text)
        compare(findChild(view, "runScore_0").text, "3400")
        compare(findChild(view, "runAccuracy_0").text, "68.0%")
        verify(findChild(view, "personalBestBadge_0").visible)
        verify(!findChild(view, "personalBestBadge_1").visible)
        // One line: every cell shares the row's vertical band.
        const row = findChild(view, "runItem_0")
        for (const name of ["runDate_0", "runScore_0", "runAccuracy_0"]) {
            const cell = findChild(view, name)
            const top = cell.mapToItem(row, 0, 0).y
            verify(top >= 0 && top + cell.height <= row.height, name + " must sit on the row's single line")
        }
        // The same column starts at the same x in every row.
        compare(findChild(view, "runScore_1").mapToItem(view, 0, 0).x,
                findChild(view, "runScore_0").mapToItem(view, 0, 0).x)
        verify(findChild(view, "runListColumnHeader").visible, "columns are titled")
        // A single-scenario list does not repeat the scenario name on every row.
        verify(!findChild(view, "runScenario_0").visible)
    }

    function test_recentRowsNameTheirScenario() {
        const view = createView({runModel: makeManyRuns(2), showScenarioName: true, showSort: false, title: ""})
        const scenario = findChild(view, "runScenario_0")
        verify(scenario.visible)
        compare(scenario.text, "Pasu Gallery")
        verify(findChild(view, "runDate_0").text.length > 0)
    }

    function test_rowExposesAnAccessibleSummary() {
        const view = createView({runModel: makeManyRuns(1)})
        const name = findChild(view, "runItem_0").Accessible.name
        verify(name.indexOf("3400") !== -1 && name.indexOf("68.0%") !== -1 && name.indexOf("personal best") !== -1, name)
    }

    // --- whole-row sizing and a visible scroll bar ---------------

    function test_heightForRowsFitsWholeRowsAndShowsScrollBarWhenOverflowing() {
        const view = createView({runModel: makeManyRuns(10), showSort: false, title: ""})
        view.height = view.heightForRows(4)
        wait(0)
        const list = findChild(view, "runListView")
        tryCompare(list, "height", 4 * view.rowHeight)
        verify(view.scrollable)
        verify(findChild(view, "runListScrollBar").visible, "an overflowing list shows its scroll bar")

        view.runModel = makeManyRuns(2)
        wait(0)
        verify(!view.scrollable)
        verify(!findChild(view, "runListScrollBar").visible)
    }
}
