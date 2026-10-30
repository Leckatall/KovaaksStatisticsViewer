import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/benchmark"

TestCase {
    id: testCase
    name: "BenchmarkGroupCommandTest"
    when: windowShown
    width: 700
    height: 560
    visible: true

    readonly property var movePasu: ({kind: "move", targetId: "cat-click", path: "Clicking"})
    readonly property var moveTracking: ({kind: "move", targetId: "cat-track", path: "Tracking"})
    readonly property var moveStatic: ({kind: "move", targetId: "sub-static", path: "Tracking › Static"})
    readonly property var exactMatchQuery: "clicking"

    property var providerCalls: []

    function fakeOptions(query, parentId) {
        providerCalls.push([query, parentId])
        const trimmed = query.trim()
        if (trimmed === "")
            return {actions: [movePasu, moveTracking, moveStatic], exactMatch: false}
        if (trimmed.toLowerCase() === exactMatchQuery)
            return {actions: [movePasu], exactMatch: true}
        const create = {kind: "create", name: trimmed, parentId: parentId}
        if (trimmed === "many") {
            const moves = []
            for (let i = 0; i < 7; ++i)
                moves.push({kind: "move", targetId: "g" + i, path: "Group " + i})
            return {actions: [create].concat(moves), exactMatch: false}
        }
        return {actions: [create, movePasu], exactMatch: false}
    }

    BenchmarkGroupCommand {
        id: popup
        optionsProvider: testCase.fakeOptions
        parentCategories: [{id: "cat-click", name: "Clicking"}, {id: "cat-track", name: "Tracking"}]
        scenarioNames: ["Pasu Gallery", "Smoothbot Gallery"]
    }

    SignalSpy { id: createSpy; target: popup; signalName: "createRequested" }
    SignalSpy { id: moveSpy; target: popup; signalName: "moveRequested" }

    function init() {
        popup.close()
        tryVerify(() => !popup.opened)
        providerCalls = []
        createSpy.clear()
        moveSpy.clear()
        popup.open()
        tryVerify(() => popup.opened)
    }

    function cleanup() {
        popup.close()
    }

    function typeQuery(text) {
        for (const character of text)
            keyClick(character)
        verify(popup.queryField.text === text)
    }

    function actionLabel(index) {
        return popup.actionList.itemAtIndex(index).text
    }

    function test_searchFieldHoldsFocusOnOpen() {
        verify(popup.queryField.activeFocus)
    }

    function test_textResetsOnReopen() {
        typeQuery("abc")
        popup.parentPicker.currentIndex = 2
        popup.close()
        tryVerify(() => !popup.opened)
        popup.open()
        tryVerify(() => popup.opened)
        compare(popup.queryField.text, "")
        compare(popup.parentPicker.currentIndex, 0)
        compare(popup.highlightedIndex, -1)
        verify(popup.queryField.activeFocus)
    }

    function test_titleCountsScenariosAndListsNames() {
        compare(popup.titleText, "Group 2 scenarios")
        compare(popup.namesText, "Pasu Gallery · Smoothbot Gallery")
    }

    function test_oneScenarioReadsInTheSingular() {
        popup.scenarioNames = ["Pasu Gallery"]
        compare(popup.titleText, "Group 1 scenario")
        typeQuery("Gallery")
        compare(actionLabel(0), "+ Create category “Gallery” and move 1 scenario")
        compare(actionLabel(1), "→ Move 1 scenario to Clicking")
        popup.scenarioNames = ["Pasu Gallery", "Smoothbot Gallery"]
    }

    function test_defaultCreateAction() {
        typeQuery("Gallery")
        compare(popup.actions.length, 2)
        compare(popup.highlightedIndex, 0)
        compare(actionLabel(0), "+ Create category “Gallery” and move 2 scenarios")
        compare(actionLabel(1), "→ Move 2 scenarios to Clicking")
        compare(popup.statusText, "")
    }

    function test_exactMatchShowsStatusAndMovesToExistingGroup() {
        typeQuery("Clicking")
        compare(popup.statusText, "Exact match found. Enter moves to the existing group.")
        compare(popup.actions.length, 1)
        keyClick(Qt.Key_Return)
        compare(createSpy.count, 0)
        compare(moveSpy.count, 1)
        compare(moveSpy.signalArguments[0][0], "cat-click")
    }

    function test_subcategoryAfterChoosingParent() {
        typeQuery("Gallery")
        popup.parentPicker.currentIndex = 1
        compare(providerCalls[providerCalls.length - 1][1], "cat-click")
        compare(actionLabel(0), "+ Create subcategory “Gallery” in Clicking and move 2 scenarios")
        keyClick(Qt.Key_Return)
        compare(createSpy.count, 1)
        compare(createSpy.signalArguments[0][0], "Gallery")
        compare(createSpy.signalArguments[0][1], "cat-click")
    }

    function test_topLevelParentPassesEmptyId() {
        typeQuery("Gallery")
        compare(providerCalls[providerCalls.length - 1][1], "")
        keyClick(Qt.Key_Return)
        compare(createSpy.signalArguments[0][1], "")
    }

    function test_downAndUpMoveWithoutWrapping() {
        typeQuery("Gallery")
        keyClick(Qt.Key_Up)
        compare(popup.highlightedIndex, 0)
        keyClick(Qt.Key_Down)
        compare(popup.highlightedIndex, 1)
        keyClick(Qt.Key_Down)
        compare(popup.highlightedIndex, 1)
        keyClick(Qt.Key_Up)
        compare(popup.highlightedIndex, 0)
    }

    function test_enterAppliesHighlightedActionAndCloses() {
        typeQuery("Gallery")
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
        compare(createSpy.count, 0)
        compare(moveSpy.count, 1)
        compare(moveSpy.signalArguments[0][0], "cat-click")
        tryVerify(() => !popup.opened)
    }

    function test_enterOnEmptyQueryDoesNothing() {
        compare(popup.highlightedIndex, -1)
        compare(popup.actions.length, 3)
        keyClick(Qt.Key_Return)
        compare(createSpy.count, 0)
        compare(moveSpy.count, 0)
        verify(popup.opened)
    }

    function test_tabReachesParentPickerWhereEnterApplies() {
        typeQuery("Gallery")
        keyClick(Qt.Key_Tab)
        verify(popup.parentPicker.activeFocus)
        keyClick(Qt.Key_Return)
        compare(createSpy.count, 1)
        compare(createSpy.signalArguments[0][0], "Gallery")
        tryVerify(() => !popup.opened)
    }

    function test_escapeClosesWithoutEmitting() {
        typeQuery("Gallery")
        keyClick(Qt.Key_Escape)
        tryVerify(() => !popup.opened)
        compare(createSpy.count, 0)
        compare(moveSpy.count, 0)
    }

    function test_clickingAnActionAppliesIt() {
        typeQuery("Gallery")
        mouseClick(popup.actionList.itemAtIndex(1))
        compare(moveSpy.count, 1)
        compare(moveSpy.signalArguments[0][0], "cat-click")
        compare(createSpy.count, 0)
        tryVerify(() => !popup.opened)
    }

    function test_uncategorizedAndEmptyTargetPassThrough() {
        popup.optionsProvider = () => ({actions: [{kind: "move", targetId: "", path: "Uncategorized"}], exactMatch: true})
        typeQuery("x")
        compare(actionLabel(0), "→ Move 2 scenarios to Uncategorized")
        keyClick(Qt.Key_Return)
        compare(moveSpy.signalArguments[0][0], "")
        popup.optionsProvider = testCase.fakeOptions
    }

    function test_eightActionsScrollAndStaySelectable() {
        typeQuery("many")
        compare(popup.actions.length, 8)
        const rowHeight = popup.actionList.itemAtIndex(0).height
        verify(popup.actionList.height <= rowHeight * 6 + 1)
        for (let i = 0; i < 9; ++i)
            keyClick(Qt.Key_Down)
        compare(popup.highlightedIndex, 7)
        const last = popup.actionList.itemAtIndex(7)
        verify(last !== null)
        verify(popup.actionList.contentY > 0)
        verify(last.y - popup.actionList.contentY >= 0)
        verify(last.y + last.height - popup.actionList.contentY <= popup.actionList.height + 1)
        keyClick(Qt.Key_Return)
        compare(moveSpy.count, 1)
        compare(moveSpy.signalArguments[0][0], "g6")
    }

    function test_popupFitsAShortParent() {
        typeQuery("many")
        verify(popup.parent.height === 560)
        verify(popup.height <= testCase.height - 36)
        verify(popup.y >= 0)
        verify(popup.y + popup.height <= testCase.height)
    }
}
