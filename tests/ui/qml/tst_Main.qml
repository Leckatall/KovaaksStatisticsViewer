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
        property string activeScenarioHash: ""
        property string activeScenarioName: ""
        property bool hasScenarios: true
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

    function cleanup() {
        fakeSettingsVm.kovaaksDirSet = true
        fakeScenarioBrowserVm.hasScenarios = true
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

    // -- the tab bar reads as main navigation ---------------------------

    function test_workspaceTabsAreComfortableClickTargets() {
        const win = createMain()
        const scenariosTab = find(win, "scenariosTab")
        const benchmarksTab = find(win, "benchmarksTab")
        verify(scenariosTab !== null && benchmarksTab !== null, "both workspace tabs must exist")

        // The old strip was ~21px tall and looked like a table header.
        verify(scenariosTab.height >= 36, "Scenarios tab is only " + scenariosTab.height + "px tall")
        verify(benchmarksTab.height >= 36, "Benchmarks tab is only " + benchmarksTab.height + "px tall")
        verify(scenariosTab.width >= 100, "Scenarios tab is only " + scenariosTab.width + "px wide")
        verify(tabBar(win).height >= 36, "the tab bar must be at least as tall as its tabs")
    }

    function test_selectedWorkspaceTabIsVisuallyDistinctFromTheOther() {
        const win = createMain()
        const scenariosTab = find(win, "scenariosTab")
        const benchmarksTab = find(win, "benchmarksTab")

        verify(scenariosTab.checked && !benchmarksTab.checked, "Scenarios starts selected")
        verify(scenariosTab.contentItem.font.bold, "the selected tab's label must be bold")
        verify(!benchmarksTab.contentItem.font.bold, "the unselected tab's label must not be bold")
        verify(Qt.colorEqual(scenariosTab.contentItem.color, scenariosTab.palette.text),
               "the selected tab's label must use the full text colour")
        verify(!Qt.colorEqual(benchmarksTab.contentItem.color, scenariosTab.contentItem.color),
               "the unselected tab's label must be dimmed relative to the selected one")
        verify(!Qt.colorEqual(scenariosTab.background.color, benchmarksTab.background.color),
               "the selected tab must have a different background from the unselected one")

        tabBar(win).currentIndex = 1
        tryVerify(() => benchmarksTab.checked && !scenariosTab.checked)
        verify(benchmarksTab.contentItem.font.bold, "the emphasis must follow the selection")
        verify(!scenariosTab.contentItem.font.bold, "the emphasis must leave the previously selected tab")
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
    // -- first run puts choosing the folder first ----------------------

    // Every item under `root` whose QML type name starts with `typeName`.
    function findAllOfType(root, typeName, matches) {
        matches = matches || []
        if (!root)
            return matches
        if (String(root).startsWith(typeName))
            matches.push(root)
        for (const child of root.children || [])
            findAllOfType(child, typeName, matches)
        return matches
    }

    function isShown(item) {
        for (let it = item; it; it = it.parent)
            if (!it.visible) return false
        return true
    }

    function test_firstRunWithNothingImportedShowsOnlyTheWelcome() {
        fakeSettingsVm.kovaaksDirSet = false
        fakeScenarioBrowserVm.hasScenarios = false
        const win = createMain()

        const welcome = find(win, "firstRunWelcome")
        verify(welcome !== null, "no firstRunWelcome in the Scenarios workspace")
        verify(isShown(welcome), "the first-run welcome must be shown")
        const button = ItemLookup.findByObjectName(welcome, "welcomeChooseKovaaksFolderButton")
        verify(button !== null && isShown(button), "the welcome must offer choosing the folder")

        const workspace = find(win, "scenarioWorkspace")
        for (const type of ["DashboardGraphCanvas", "PlaytimeGraphPanel", "ScenarioHistoryPanel", "SelectionPanel"]) {
            const items = findAllOfType(workspace, type)
            verify(items.length > 0, "expected the workspace to contain a " + type)
            for (const item of items)
                verify(!isShown(item), type + " must not be shown on first run")
        }
        verify(!isShown(ItemLookup.findByObjectName(workspace, "scenarioSearchPanel")),
               "the empty scenario list must not be shown behind the welcome")
        verify(!find(win, "firstRunBanner").visible,
               "the banner would only repeat the welcome")

        welcomeSpy.target = workspace
        button.clicked()
        compare(welcomeSpy.count, 1, "the welcome button must ask for the folder dialog")
    }

    SignalSpy {
        id: welcomeSpy
        signalName: "chooseKovaaksFolderRequested"
    }

    function test_firstRunBannerHidesOnTheBenchmarksTabToo() {
        fakeSettingsVm.kovaaksDirSet = false
        fakeScenarioBrowserVm.hasScenarios = false
        const win = createMain()
        const banner = find(win, "firstRunBanner")
        const barYOnScenarios = tabBar(win).y

        tabBar(win).currentIndex = 1
        tryCompare(stack(win), "currentIndex", 1)
        compare(banner.visible, false)
        compare(tabBar(win).y, barYOnScenarios, "switching tabs must not move the tab bar")
    }

    function test_folderNotSetButRunsAvailableShowsWorkspaceAndBanner() {
        fakeSettingsVm.kovaaksDirSet = false
        fakeScenarioBrowserVm.hasScenarios = true
        const win = createMain()

        verify(!isShown(find(win, "firstRunWelcome")), "imported runs must not be hidden behind the welcome")
        verify(isShown(ItemLookup.findByObjectName(find(win, "scenarioWorkspace"), "scenarioSearchPanel")))
        verify(find(win, "firstRunBanner").visible, "the banner still asks for the folder")
    }

    function test_configuredFolderShowsTheWorkspaceWithoutWelcomeOrBanner() {
        fakeSettingsVm.kovaaksDirSet = true
        fakeScenarioBrowserVm.hasScenarios = false
        const win = createMain()

        verify(!isShown(find(win, "firstRunWelcome")))
        verify(!find(win, "firstRunBanner").visible)
        verify(isShown(ItemLookup.findByObjectName(find(win, "scenarioWorkspace"), "scenarioSearchPanel")))
    }
}
