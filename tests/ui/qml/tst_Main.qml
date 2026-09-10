import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

// W1: the main window is a Scenarios/Benchmarks shell. These tests fail until
// Main.qml grows a top TabBar + workspace StackLayout, an extracted
// ScenarioWorkspace, a persistent BenchmarkWorkspace, and a required
// `benchmarkTrackingVm` initial property.
TestCase {
    id: testCase
    name: "MainWorkspaceShellTest"
    when: windowShown
    width: 1200
    height: 800
    visible: true

    // --- fake view models Main.qml wires ------------------------------------

    QtObject {
        id: fakeGraphVm
        signal seriesConfigurationChanged()
        property var enabledSeriesIds: []
        property var allSeries: []
        property var fetchData: function () {}
        property var columnForSeriesId: function () { return -1 }
        property var seriesIdForColumn: function () { return "" }
    }

    QtObject {
        id: fakeSettingsVm
        property string kovaaksDir: ""
        property bool kovaaksDirSet: true
        property var setKovaaksDir: function () {}
    }

    QtObject {
        id: fakeScenarioBrowserVm
        property string longestScenarioName: ""
        property string currentRunHash: ""
        property double currentRunStartTimeMs: 0
        property var scenarioModel: null
        property var runModel: null
        property var recentRunsModel: null
        property var setSearchText: function () {}
        property var activateScenario: function () {}
        property var selectRun: function () {}
        property var setRunSort: function () {}
        property var setScenarioSort: function () {}
    }

    QtObject { id: fakePlaytimeVm }
    QtObject { id: fakeSessionVm }
    property var fakeHistoryVm: TestDoubles.makeFakeHistoryVm()
    property var fakeManagerVm: TestDoubles.makeFakeBenchmarkManagerVm({})
    property var fakeTrackingVm: TestDoubles.makeFakeTrackingVm(TestDoubles.trackingFixtureNoSelection())

    Component {
        id: mainComponent
        Main {}
    }

    function createMain() {
        const win = createTemporaryObject(mainComponent, testCase, {
            graphVm: fakeGraphVm,
            playtimeVm: fakePlaytimeVm,
            historyVm: fakeHistoryVm,
            sessionVm: fakeSessionVm,
            settingsVm: fakeSettingsVm,
            scenarioBrowserVm: fakeScenarioBrowserVm,
            benchmarkManagerVm: fakeManagerVm,
            benchmarkTrackingVm: fakeTrackingVm
        })
        verify(win !== null, "Main.qml failed to instantiate")
        tryVerify(() => win.contentItem !== null && win.contentItem.width > 0,
                  2000, "Main window never rendered")
        return win
    }

    function find(win, objectName) {
        return ItemLookup.findByObjectName(win.contentItem, objectName)
            || ItemLookup.findByObjectName(win, objectName)
    }

    function tabBar(win) {
        const tb = find(win, "workspaceTabBar")
        verify(tb !== null, "no workspaceTabBar in Main.qml - the shell has no top tab bar")
        return tb
    }

    function stack(win) {
        const st = find(win, "workspaceStack")
        verify(st !== null, "no workspaceStack in Main.qml - the shell has no workspace StackLayout")
        return st
    }

    // -- required property --------------------------------------------------

    function test_mainConsumesRequiredBenchmarkTrackingVm() {
        const win = createMain()
        verify(win.benchmarkTrackingVm !== undefined && win.benchmarkTrackingVm !== null,
               "Main.qml does not expose a required `benchmarkTrackingVm` property")
        compare(win.benchmarkTrackingVm, fakeTrackingVm,
                "Main.qml must consume the injected benchmarkTrackingVm")
    }

    // -- initial tab ------------------------------------------------------------

    function test_scenariosIsTheInitialWorkspace() {
        const win = createMain()
        compare(tabBar(win).currentIndex, 0, "Scenarios must be the initial tab")
        compare(stack(win).currentIndex, 0, "workspace stack must start on the Scenarios page")

        const scenario = find(win, "scenarioWorkspace")
        const benchmark = find(win, "benchmarkWorkspace")
        verify(scenario !== null, "no scenarioWorkspace - the scenario dashboard was not extracted")
        verify(benchmark !== null, "no benchmarkWorkspace child in the stack")
        compare(scenario.visible, true, "Scenarios workspace should be visible at startup")
        compare(benchmark.visible, false, "Benchmarks workspace should be hidden at startup")
    }

    // -- accessible names + keyboard switching --------------------------------

    function test_bothTabsHaveAccessibleNames() {
        const win = createMain()
        const scenariosTab = find(win, "scenariosTab")
        const benchmarksTab = find(win, "benchmarksTab")
        verify(scenariosTab !== null && benchmarksTab !== null, "both workspace tabs must exist")
        compare(scenariosTab.Accessible.name, "Scenarios")
        compare(benchmarksTab.Accessible.name, "Benchmarks")
    }

    function test_keyboardSwitchesBetweenWorkspaces() {
        const win = createMain()
        win.requestActivate()
        tryVerify(() => win.active, 2000, "the Main window never became the active window")
        const tb = tabBar(win)
        tb.forceActiveFocus()
        tryVerify(() => tb.activeFocus, 2000, "the workspace tab bar could not take keyboard focus")
        compare(tb.currentIndex, 0)

        keyClick(Qt.Key_Right)
        tryCompare(tb, "currentIndex", 1, 2000, "Right arrow must move focus/selection to the Benchmarks tab")
        tryCompare(stack(win), "currentIndex", 1, 2000, "stack must follow the tab bar")

        keyClick(Qt.Key_Left)
        tryCompare(tb, "currentIndex", 0, 2000, "Left arrow must return to the Scenarios tab")
    }

    // -- state retention across tab switches ---------------------------------

    function test_switchingTabsPreservesWorkspaceInstancesAndState() {
        const win = createMain()
        const st = stack(win)
        const scenarioBefore = find(win, "scenarioWorkspace")
        const benchmarkBefore = find(win, "benchmarkWorkspace")
        verify(scenarioBefore !== null && benchmarkBefore !== null)

        // Mutate a bit of session-only state that must survive a round trip.
        const visualSettings = find(win, "visualSettings")
        verify(visualSettings !== null, "visualSettings object should be reachable")
        visualSettings.scenarioGraphVisible = false
        fakeTrackingVm.selectBenchmark("bench-A")

        tabBar(win).currentIndex = 1
        tryCompare(st, "currentIndex", 1)
        tabBar(win).currentIndex = 0
        tryCompare(st, "currentIndex", 0)

        compare(find(win, "scenarioWorkspace"), scenarioBefore,
                "the Scenarios workspace must not be destroyed by a tab switch")
        compare(find(win, "benchmarkWorkspace"), benchmarkBefore,
                "the Benchmarks workspace must not be destroyed by a tab switch")
        compare(visualSettings.scenarioGraphVisible, false,
                "visual settings must survive a tab switch")
        compare(fakeTrackingVm.selectBenchmarkCalls.length, 1,
                "the tracking view model (session benchmark selection) must not be recreated")
    }

    // -- manager entry points target the same dialog -----------------------

    function test_managerOpensFromMenuAndWorkspaceAgainstOneDialog() {
        const win = createMain()

        const benchmarkWorkspace = find(win, "benchmarkWorkspace")
        verify(benchmarkWorkspace !== null, "no benchmarkWorkspace to open the manager from")

        // The manager is a separate ApplicationWindow, reached through the shell's
        // alias rather than the item-tree walk `find` performs.
        const dialog = win.benchmarkManagerDialog
        verify(dialog !== null && dialog !== undefined,
               "the shell exposes no shared benchmarkManagerDialog")

        // Menu path.
        win.menuBar.manageBenchmarksRequested()
        tryCompare(dialog, "visible", true)
        dialog.close()
        tryCompare(dialog, "visible", false)

        // Workspace path.
        benchmarkWorkspace.manageBenchmarksRequested()
        tryCompare(dialog, "visible", true)
        compare(win.benchmarkManagerDialog, dialog,
                "both entry points must open the same shared manager dialog instance")
    }
}
