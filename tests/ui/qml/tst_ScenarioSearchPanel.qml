import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "ScenarioSearchPanelTest"
    when: windowShown
    width: 700
    height: 500
    visible: true

    Component {
        id: viewComponent
        ScenarioSearchPanel {}
    }

    SignalSpy {
        id: sortRequestedSpy
        signalName: "scenarioSortRequested"
    }

    function makeScenarios() {
        return [
            {hash: "hash-1", name: "1wall6targets TE", runCount: 3},
            {hash: "hash-2", name: "Microshot", runCount: 7},
        ]
    }

    function createView(props) {
        const view = createTemporaryObject(viewComponent, testCase,
            Object.assign({width: 650, height: 420, scenarioModel: makeScenarios()}, props))
        verify(waitForRendering(view))
        return view
    }

    function test_sortFieldSelectionEmitsScenarioSortRequested() {
        const view = createView({})

        const combo = findChild(view, "scenarioSortCombo")
        verify(combo !== null)
        combo.forceActiveFocus()

        // Material's ComboBox can emit `activated` more than once for a single
        // keyboard selection; track calls directly instead of via SignalSpy so
        // multiple identical emissions don't break argument indexing.
        const captured = []
        const onSort = (field, ascending) => captured.push([field, ascending])
        view.scenarioSortRequested.connect(onSort)

        keyClick(Qt.Key_Down)
        wait(50)
        view.scenarioSortRequested.disconnect(onSort)

        verify(captured.length >= 1)
        const lastArgs = captured[captured.length - 1]
        compare(lastArgs[0], 1)
        compare(lastArgs[1], false)
    }

    function test_sortDirectionToggleEmitsScenarioSortRequestedWithFlippedDirection() {
        const view = createView({})
        const dirButton = findChild(view, "scenarioSortDirectionButton")
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
