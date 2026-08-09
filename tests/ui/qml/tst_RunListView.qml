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

    function makeRuns() {
        return [{hash: "scenario-a", runLabel: "run", scenarioName: "Scenario A", startTimeMs: 1723200000000,
                 score: 8421, accuracy: 0.91, durationSeconds: 60, shots: 132, hits: 120}]
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
}
