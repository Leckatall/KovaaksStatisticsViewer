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
        return [{hash: "scenario-a", runLabel: "run", startTimeMs: 1723200000000,
                 score: 8421, accuracy: 0.91, shots: 132, hits: 120}]
    }

    function makeSortableRuns() {
        return [
            {hash: "a", runLabel: "run-a", startTimeMs: 3000, score: 8000, accuracy: 0.80, shots: 100, hits: 80},
            {hash: "b", runLabel: "run-b", startTimeMs: 1000, score: 9500, accuracy: 0.95, shots: 100, hits: 95},
            {hash: "c", runLabel: "run-c", startTimeMs: 2000, score: 7000, accuracy: 0.70, shots: 100, hits: 70},
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
        verify(findChild(delegate, "runLabel_0").text.length > 0)

        runSelectedSpy.target = view
        verify(runSelectedSpy.valid)
        mouseClick(delegate, delegate.width / 2, delegate.height / 2)
        wait(0)
        compare(runSelectedSpy.count, 1)
        compare(runSelectedSpy.signalArguments[0][0], "scenario-a")
        compare(runSelectedSpy.signalArguments[0][1], 1723200000000)
    }

    function test_emptyModelShowsEmptyLabel() {
        const view = createView({runModel: null})
        verify(view.hasNoRuns())
        verify(findChild(view, "runListEmptyLabel").visible)
    }

    function test_sortFieldSelectionEmitsSortRequestedAndReordersRows() {
        const view = createView({runModel: makeSortableRuns()})
        verify(findChild(view, "runLabel_0").text.indexOf("run-a") !== -1)

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
            {hash: "b", runLabel: "run-b", startTimeMs: 1000, score: 9500, accuracy: 0.95, shots: 100, hits: 95},
            {hash: "a", runLabel: "run-a", startTimeMs: 3000, score: 8000, accuracy: 0.80, shots: 100, hits: 80},
            {hash: "c", runLabel: "run-c", startTimeMs: 2000, score: 7000, accuracy: 0.70, shots: 100, hits: 70},
        ]
        wait(0)

        verify(findChild(view, "runLabel_0").text.indexOf("run-b") !== -1)
        verify(findChild(view, "runLabel_1").text.indexOf("run-a") !== -1)
        verify(findChild(view, "runLabel_2").text.indexOf("run-c") !== -1)
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
}
