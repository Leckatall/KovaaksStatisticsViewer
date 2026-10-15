import QtQuick
import QtTest
import "../../../src/ui/qml/scenario"

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
    // --- the empty list says what to do, where the user is looking ------

    function test_emptyCatalogueShowsGuidanceAtTheTopOfTheList() {
        const view = createView({scenarioModel: [], height: 600})
        const state = findChild(view, "scenarioListEmptyState")
        verify(state !== null && state.visible)
        const list = findChild(view, "scenarioListView")
        const top = state.mapToItem(list, 0, 0).y
        verify(top < list.height / 3, "the empty state must sit in the upper part of the list (y=" + top + ")")
        verify(findChild(view, "scenarioListEmptyLabel").text.length > 0)
        const hint = findChild(view, "scenarioListEmptyHint").text
        verify(hint.indexOf("Kovaaks folder") !== -1, "the hint must say how runs get here: " + hint)
    }

    function test_searchWithNoMatchesSaysSoInsteadOfTheFirstRunHint() {
        const view = createView({scenarioModel: []})
        view.searchText = "zzz"
        wait(0)
        verify(findChild(view, "scenarioListEmptyLabel").text.indexOf("zzz") !== -1)
        verify(findChild(view, "scenarioListEmptyHint").text.indexOf("Clear the search") !== -1)
    }

    function test_populatedListHidesTheEmptyState() {
        const view = createView({})
        verify(!findChild(view, "scenarioListEmptyState").visible)
    }

    // --- the highlight follows the externally driven active scenario ----

    function test_highlightFollowsCurrentScenarioHashAndClickOnlyEmits() {
        const view = createView({currentScenarioHash: "hash-1"})
        const first = findChild(view, "scenarioItem_0")
        const second = findChild(view, "scenarioItem_1")
        verify(first.highlighted)
        verify(!second.highlighted)

        const activated = []
        view.scenarioActivated.connect((hash, name) => activated.push([hash, name]))
        mouseClick(second, second.width / 2, second.height / 2)
        wait(0)
        compare(activated.length, 1)
        compare(activated[0][0], "hash-2")
        compare(activated[0][1], "Microshot")
        compare(view.currentScenarioHash, "hash-1", "the view model, not the click, moves the highlight")

        view.currentScenarioHash = "hash-2"
        wait(0)
        verify(second.highlighted)
        verify(!first.highlighted)
    }
}
