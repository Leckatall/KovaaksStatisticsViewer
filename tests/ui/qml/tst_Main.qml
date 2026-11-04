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
        property bool kovaaksDirValid: true
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
    QtObject {
        id: fakeCaptureVm
        signal previewClosed()
        property int themeCalls: 0
        property var lastTheme: null
        property var lastFont: null
        function useTheme(theme, font) {
            themeCalls++
            lastTheme = theme
            lastFont = font
        }
    }
    QtObject {
        id: fakeSessionVm
        property int refreshCalls: 0
        property bool profileImportBusy: false
        property string profileImportOutcome: "none"
        property var profileImportIssues: []
        property bool profileImportSaveFailed: false
        function refreshProfile() { refreshCalls++ }
    }
    property var fakeHistoryVm: TestDoubles.makeFakeHistoryVm()
    property var fakeManagerVm: TestDoubles.makeFakeBenchmarkManagerVm({})
    property var fakeTrackingVm: TestDoubles.makeFakeTrackingVm(TestDoubles.trackingFixtureNoSelection())

    Component {
        id: mainComponent
        Main {}
    }

    function cleanup() {
        fakeCaptureVm.themeCalls = 0
        fakeSessionVm.refreshCalls = 0
        fakeSessionVm.profileImportSaveFailed = false
        fakeSettingsVm.kovaaksDirValid = true
        fakeScenarioBrowserVm.hasScenarios = true
    }

    function test_fileRefreshUsesTheSessionVm() {
        const win = createMain()
        const menu = win.menuBar.menuAt(0)
        let refresh = null
        for (let i = 0; i < menu.count; ++i) {
            const item = menu.itemAt(i)
            if (item && item.action && item.action.objectName === "refreshProfileAction") refresh = item.action
        }
        verify(refresh !== null)
        refresh.trigger()
        compare(fakeSessionVm.refreshCalls, 1)
    }

    function test_theThemeReachesTheCaptureViewModel() {
        const win = createMain()
        compare(fakeCaptureVm.themeCalls, 1)
        verify(fakeCaptureVm.lastTheme !== null)
        compare(fakeCaptureVm.lastFont.family, win.font.family)
        compare(fakeCaptureVm.lastFont.pixelSize, win.font.pixelSize)
    }

    function test_profileStatusBarIsHiddenWhileIdle() {
        const win = createMain()
        const bar = ItemLookup.findByObjectName(win.contentItem, "profileStatusBar")
        verify(bar !== null, "Main.qml has no profile status bar")
        compare(bar.visible, false)
    }

    function test_theStatusBarOverlaysTheContentWithoutMovingIt() {
        const win = createMain()
        const bar = ItemLookup.findByObjectName(win.contentItem, "profileStatusBar")
        const workspaces = stack(win)
        const before = workspaces.mapToItem(null, 0, 0)
        const heightBefore = workspaces.height
        fakeSessionVm.profileImportSaveFailed = true
        compare(bar.visible, true)
        const after = workspaces.mapToItem(null, 0, 0)
        compare(workspaces.height, heightBefore)
        compare(after.y, before.y)
        const barBottom = bar.mapToItem(win.contentItem, 0, bar.height).y
        verify(Math.abs(barBottom - win.contentItem.height) < 1, "the bar sits at the bottom of the window")
        verify(bar.z > workspaces.z, "the bar draws over the content")
    }

    function test_previewCloseFallsBackToTheWorkspace() {
        const win = createMain()
        win.requestActivate()
        tryVerify(() => win.active, 2000, "the Main window never became the active window")
        tabBar(win).forceActiveFocus()
        const workspaces = stack(win)
        const inside = item => { for (let p = item; p; p = p.parent) if (p === workspaces) return true; return false }
        verify(!inside(win.activeFocusItem), "focus must start outside the workspaces")
        fakeCaptureVm.previewClosed()
        tryVerify(() => inside(win.activeFocusItem), 2000, "focus did not fall back into the workspace")
    }

    function createMain(managerVm) {
        const win = createTemporaryObject(mainComponent, testCase, {
            graphVm: fakeGraphVm,
            playtimeVm: fakePlaytimeVm,
            historyVm: fakeHistoryVm,
            sessionVm: fakeSessionVm,
            settingsVm: fakeSettingsVm,
            scenarioBrowserVm: fakeScenarioBrowserVm,
            benchmarkManagerVm: managerVm || fakeManagerVm,
            benchmarkTrackingVm: fakeTrackingVm,
            captureVm: fakeCaptureVm
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

    // -- initial tab ------------------------------------------------------------

    // -- the tab bar reads as main navigation ---------------------------

    // -- accessible names + keyboard switching --------------------------------

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

    // Ctrl+F searches scenarios from the Benchmarks tab too: back to Scenarios, caret in the field.
    function test_ctrlFFocusesTheScenarioSearchFromEitherTab() {
        const win = createMain()
        win.requestActivate()
        tryVerify(() => win.active, 2000, "the Main window never became the active window")
        const tb = tabBar(win)
        tb.currentIndex = 1
        tb.forceActiveFocus()
        tryCompare(stack(win), "currentIndex", 1)

        keyClick(Qt.Key_F, Qt.ControlModifier)

        tryCompare(tb, "currentIndex", 0, 2000, "Ctrl+F must switch to the Scenarios tab")
        const field = find(win, "scenarioSearchField")
        verify(field !== null)
        tryVerify(() => field.activeFocus, 2000, "Ctrl+F must focus the scenario search field")
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
    // A warning row in the Benchmarks tab's selector opens the shared manager on its benchmark.
    function test_selectorWarningOpensTheManagerOnThatBenchmark() {
        const win = createMain(TestDoubles.makeFakeBenchmarkManagerVm({libraryEntries: [
            {id: "bench-A", name: "Alpha", filename: "a.json", classification: "Incomplete",
             openable: true, deletable: true},
            {id: "", name: "bad.json", filename: "bad.json", classification: "Invalid",
             openable: false, deletable: false}
        ]}))
        const benchmarkWorkspace = find(win, "benchmarkWorkspace")
        const dialog = win.benchmarkManagerDialog

        benchmarkWorkspace.editBenchmarkRequested("bad.json", "")
        tryCompare(dialog, "visible", true)
        compare(dialog.selectedEntryFilename, "bad.json")
        compare(dialog.editorTable.manager.openCalls.length, 0, "a problem file is only selected")
        dialog.close()
        tryCompare(dialog, "visible", false)

        benchmarkWorkspace.editBenchmarkRequested("a.json", "bench-A")
        tryCompare(dialog, "visible", true)
        compare(dialog.selectedEntryFilename, "a.json")
        compare(dialog.editorTable.manager.openCalls, ["bench-A"], "a loaded benchmark opens in the editor")
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
        fakeSettingsVm.kovaaksDirValid = false
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
        verify(!find(win, "invalidKovaaksDirBanner").visible,
               "the banner would only repeat the welcome")

        welcomeSpy.target = workspace
        button.clicked()
        compare(welcomeSpy.count, 1, "the welcome button must ask for the folder dialog")
    }

    SignalSpy {
        id: welcomeSpy
        signalName: "chooseKovaaksFolderRequested"
    }

    function test_invalidFolderBannerHidesOnTheBenchmarksTabToo() {
        fakeSettingsVm.kovaaksDirValid = false
        fakeScenarioBrowserVm.hasScenarios = false
        const win = createMain()
        const banner = find(win, "invalidKovaaksDirBanner")
        const barYOnScenarios = tabBar(win).y

        tabBar(win).currentIndex = 1
        tryCompare(stack(win), "currentIndex", 1)
        compare(banner.visible, false)
        compare(tabBar(win).y, barYOnScenarios, "switching tabs must not move the tab bar")
    }

    function test_invalidFolderWithRunsShowsWorkspaceAndBanner() {
        fakeSettingsVm.kovaaksDirValid = false
        fakeScenarioBrowserVm.hasScenarios = true
        const win = createMain()

        verify(!isShown(find(win, "firstRunWelcome")), "imported runs must not be hidden behind the welcome")
        verify(isShown(ItemLookup.findByObjectName(find(win, "scenarioWorkspace"), "scenarioSearchPanel")))
        verify(find(win, "invalidKovaaksDirBanner").visible, "the banner reports the invalid folder")
    }

    function test_validFolderShowsTheWorkspaceWithoutWelcomeOrBanner() {
        fakeSettingsVm.kovaaksDirValid = true
        fakeScenarioBrowserVm.hasScenarios = false
        const win = createMain()

        verify(!isShown(find(win, "firstRunWelcome")))
        verify(!find(win, "invalidKovaaksDirBanner").visible)
        verify(isShown(ItemLookup.findByObjectName(find(win, "scenarioWorkspace"), "scenarioSearchPanel")))
    }
}
