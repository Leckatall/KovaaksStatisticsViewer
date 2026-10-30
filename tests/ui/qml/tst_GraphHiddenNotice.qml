import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "GraphHiddenNoticeTest"
    when: windowShown
    width: 500
    height: 300
    visible: true

    FakeGraphVisibility { id: visibility }

    Column {
        id: host
        width: testCase.width

        Button { objectName: "workspaceTabBar"; text: "Tabs" }
        Button {
            objectName: "graphGear_dailyPlaytime"
            text: "Gear"
            visible: visibility.playtimeGraphVisible
        }
        GraphHiddenNotice {
            id: notice
            width: parent.width
            graphKey: "dailyPlaytime"
            visualSettings: visibility
        }
    }

    function init() {
        host.visible = true
        visibility.playtimeGraphVisible = true
        visibility.hideNoticeGraph = ""
        testCase.Window.window.requestActivate()
        tryVerify(() => testCase.Window.window.active)
    }

    function named(name) { return ItemLookup.findByObjectName(host, name) }

    function quickHide() {
        visibility.quickHideGraph("dailyPlaytime")
        tryVerify(() => named("graphHiddenUndo").activeFocus)
        verify(waitForRendering(notice))
        const undo = named("graphHiddenUndo")
        verify(named("graphHiddenDismiss").x >= undo.x + undo.width, "Dismiss sits beside Undo")
    }

    function test_appearsOnlyForItsOwnGraph() {
        compare(notice.visible, false)
        visibility.quickHideGraph("run")
        compare(notice.visible, false)
        visibility.quickHideGraph("dailyPlaytime")
        compare(notice.visible, true)
        compare(named("graphHiddenNoticeTitle").text, "Daily playtime hidden")
    }

    function test_quickHideFocusesUndo() {
        quickHide()
        verify(named("graphHiddenUndo").visualFocus)
    }

    function test_undoShowsTheGraphAndReturnsFocusToItsGear() {
        quickHide()
        keyClick(Qt.Key_Space)
        compare(visibility.playtimeGraphVisible, true)
        compare(notice.visible, false)
        tryVerify(() => named("graphGear_dailyPlaytime").activeFocus)
    }

    function test_dismissLeavesTheGraphHiddenAndFocusesASurvivingControl() {
        quickHide()
        mouseClick(named("graphHiddenDismiss"))
        compare(visibility.playtimeGraphVisible, false)
        compare(notice.visible, false)
        tryVerify(() => named("workspaceTabBar").activeFocus)
    }

    function test_escapeDismisses() {
        quickHide()
        keyClick(Qt.Key_Escape)
        compare(visibility.playtimeGraphVisible, false)
        compare(notice.visible, false)
    }

    // The notice of a graph on the other workspace tab must not take Escape from the one in view.
    function test_escapeIsInertWhileTheNoticeIsOutOfView() {
        quickHide()
        host.visible = false
        keyClick(Qt.Key_Escape)
        compare(visibility.hideNoticeGraph, "dailyPlaytime")
    }
}
