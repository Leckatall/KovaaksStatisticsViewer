import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "SelectionPanelTest"
    when: windowShown
    width: 700
    height: 800
    visible: true

    Component {
        id: panelComponent
        SelectionPanel {}
    }

    SignalSpy {
        id: runSelectedSpy
        signalName: "runSelected"
    }

    function makeRuns(hash) {
        return [{hash: hash, scenarioName: "Scenario " + hash, runLabel: "run", startTimeMs: 1723200000000,
                 score: 8421, accuracy: 0.91, shots: 132, hits: 120, personalBest: false}]
    }

    // Stands in for ScenarioBrowserViewModel: an activation comes back as the
    // panel's active scenario.
    function actLikeTheViewModel(panel) {
        panel.scenarioActivated.connect((hash, name) => {
            panel.activeScenarioHash = hash
            panel.activeScenarioName = name
        })
        return panel
    }

    function createPanel() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 600, height: 700,
            scenarioModel: [{name: "Scenario A", hash: "a", runCount: 3, lastPlayedMs: 1723200000000}],
            runModel: makeRuns("a"), recentRunModel: makeRuns("recent")
        })
        verify(waitForRendering(panel))
        return actLikeTheViewModel(panel)
    }

    function test_widthIsUnaffectedByScenarioAndRunSelection() {
        const longName = "A".repeat(200)
        const panel = createTemporaryObject(panelComponent, testCase, {
            height: 700, maximumPanelWidth: 400,
            scenarioModel: [{name: longName, hash: "a", runCount: 3, lastPlayedMs: 1723200000000}],
            runModel: makeRuns("a"), recentRunModel: makeRuns("recent")
        })
        verify(waitForRendering(panel))
        actLikeTheViewModel(panel)
        const initialWidth = panel.desiredWidth

        const scenarioItem = findChild(panel, "scenarioItem_0")
        mouseClick(scenarioItem, scenarioItem.width / 2, scenarioItem.height / 2)
        wait(0)
        compare(panel.activeScenarioName, longName)
        compare(panel.desiredWidth, initialWidth)

        const runItem = findChild(findChild(panel, "scenarioRunsView"), "runItem_0")
        verify(waitForRendering(runItem))
        mouseClick(runItem, runItem.width / 2, runItem.height / 2)
        wait(0)
        compare(panel.desiredWidth, initialWidth)
    }

    function test_widthTracksWidestScenarioNameWithinBounds() {
        // The bound sits above the panel's chrome (header row with sort controls) so
        // that an empty name falls back to exactly the chrome width.
        const panel = createTemporaryObject(panelComponent, testCase, {
            height: 700, maximumPanelWidth: 480, widestScenarioName: "A".repeat(200)
        })
        verify(waitForRendering(panel))
        verify(panel.chromeWidth < 480, "chrome " + panel.chromeWidth)
        compare(panel.desiredWidth, 480)

        panel.widestScenarioName = ""
        wait(0)
        compare(panel.desiredWidth, panel.chromeWidth)
        verify(panel.chromeWidth > 0)
    }

    function test_activationRevealsScenarioRunsAndRecentRenders() {
        const panel = createPanel()
        const scenarioRuns = findChild(panel, "scenarioRunsView")
        const recentRuns = findChild(panel, "recentRunsView")
        compare(panel.recentExpanded, false)
        compare(recentRuns.visible, false)
        compare(scenarioRuns.visible, false)

        const recentToggle = findChild(panel, "recentToggleButton")
        mouseClick(recentToggle, recentToggle.width / 2, recentToggle.height / 2)
        wait(0)
        compare(recentRuns.visible, true)
        verify(findChild(recentRuns, "runItem_0") !== null)

        const scenarioItem = findChild(panel, "scenarioItem_0")
        mouseClick(scenarioItem, scenarioItem.width / 2, scenarioItem.height / 2)
        wait(0)
        compare(panel.activeScenarioName, "Scenario A")
        compare(scenarioRuns.visible, true)
    }

    function test_forwardsRunSelectionFromRecentAndScenarioLists() {
        const panel = createPanel()
        runSelectedSpy.target = panel
        verify(runSelectedSpy.valid)

        const recentToggle = findChild(panel, "recentToggleButton")
        mouseClick(recentToggle, recentToggle.width / 2, recentToggle.height / 2)
        tryVerify(() => findChild(panel, "recentRunsView").visible)
        const recentItem = findChild(findChild(panel, "recentRunsView"), "runItem_0")
        verify(waitForRendering(recentItem))
        mouseClick(recentItem, recentItem.width / 2, recentItem.height / 2)
        wait(0)
        compare(runSelectedSpy.count, 1)
        compare(runSelectedSpy.signalArguments[0][0], "recent")

        const scenarioItem = findChild(panel, "scenarioItem_0")
        mouseClick(scenarioItem, scenarioItem.width / 2, scenarioItem.height / 2)
        tryVerify(() => findChild(findChild(panel, "scenarioRunsView"), "runItem_0") !== null)
        const scenarioRunItem = findChild(findChild(panel, "scenarioRunsView"), "runItem_0")
        scenarioRunItem.click()
        wait(0)
        compare(runSelectedSpy.count, 2)
        compare(runSelectedSpy.signalArguments[1][0], "a")
    }

    function test_currentRunIsHighlightedAcrossListsAndUpdates() {
        const currentTime = 1723200000000
        const nextTime = currentTime + 1
        const runs = [
            {hash: "a", scenarioName: "Scenario A", runLabel: "current", startTimeMs: currentTime,
             score: 8421, accuracy: 0.91, shots: 132, hits: 120, personalBest: false},
            {hash: "a", scenarioName: "Scenario A", runLabel: "next", startTimeMs: nextTime,
             score: 8150, accuracy: 0.885, shots: 128, hits: 113, personalBest: false}
        ]
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 600, height: 700, currentRunHash: "a", currentRunStartTimeMs: currentTime,
            scenarioModel: [{name: "Scenario A", hash: "a", runCount: 2, lastPlayedMs: currentTime}],
            runModel: runs, recentRunModel: runs
        })
        verify(waitForRendering(panel))
        actLikeTheViewModel(panel)

        const recentToggle = findChild(panel, "recentToggleButton")
        mouseClick(recentToggle, recentToggle.width / 2, recentToggle.height / 2)
        const scenarioItem = findChild(panel, "scenarioItem_0")
        mouseClick(scenarioItem, scenarioItem.width / 2, scenarioItem.height / 2)
        const recentRuns = findChild(panel, "recentRunsView")
        const scenarioRuns = findChild(panel, "scenarioRunsView")
        tryVerify(() => findChild(recentRuns, "runItem_1") !== null
                        && findChild(scenarioRuns, "runItem_1") !== null)

        const recentCurrent = findChild(recentRuns, "runItem_0")
        const recentNext = findChild(recentRuns, "runItem_1")
        const scenarioCurrent = findChild(scenarioRuns, "runItem_0")
        const scenarioNext = findChild(scenarioRuns, "runItem_1")
        compare(recentCurrent.isCurrentRun, true)
        compare(scenarioCurrent.isCurrentRun, true)
        compare(recentNext.isCurrentRun, false)
        compare(scenarioNext.isCurrentRun, false)
        compare(recentCurrent.background.color, recentCurrent.currentRunColor)
        compare(recentCurrent.background.border.color, panel.palette.accent)

        panel.currentRunStartTimeMs = nextTime
        wait(0)

        compare(recentCurrent.isCurrentRun, false)
        compare(scenarioCurrent.isCurrentRun, false)
        compare(recentNext.isCurrentRun, true)
        compare(scenarioNext.isCurrentRun, true)
    }

    function test_recentSectionVisibleFalse_hidesToggleButtonAndList() {
        const panel = createPanel()
        const recentToggle = findChild(panel, "recentToggleButton")
        mouseClick(recentToggle, recentToggle.width / 2, recentToggle.height / 2)
        wait(0)
        compare(findChild(panel, "recentRunsView").visible, true)

        panel.recentSectionVisible = false
        wait(0)
        compare(recentToggle.visible, false)
        compare(findChild(panel, "recentRunsView").visible, false, "recentExpanded stays true but the section is hidden")
    }

    function test_scenarioBrowserSectionVisibleFalse_hidesSearchPanelAndAndsIntoRunListGate() {
        const panel = createPanel()
        const scenarioItem = findChild(panel, "scenarioItem_0")
        mouseClick(scenarioItem, scenarioItem.width / 2, scenarioItem.height / 2)
        wait(0)
        compare(panel.activeScenarioName, "Scenario A")
        compare(findChild(panel, "scenarioRunsView").visible, true)

        panel.scenarioBrowserSectionVisible = false
        wait(0)
        compare(findChild(panel, "scenarioSearchPanel").visible, false)
        compare(findChild(panel, "scenarioRunsView").visible, false,
                "activeScenarioName is still set but the section is hidden")

        panel.scenarioBrowserSectionVisible = true
        wait(0)
        compare(findChild(panel, "scenarioSearchPanel").visible, true)
        compare(findChild(panel, "scenarioRunsView").visible, true)
    }
    // --- the panel shows the view model's active scenario ---------------

    function test_activeScenarioIsDrivenFromOutside() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 600, height: 700,
            scenarioModel: [{name: "Scenario A", hash: "a", runCount: 3, lastPlayedMs: 1723200000000},
                            {name: "Scenario B", hash: "b", runCount: 1, lastPlayedMs: 1723200000000}],
            runModel: makeRuns("b"), recentRunModel: makeRuns("b"),
            activeScenarioHash: "b", activeScenarioName: "Scenario B"
        })
        verify(waitForRendering(panel))
        const runs = findChild(panel, "scenarioRunsView")
        verify(runs.visible, "an active scenario from the view model lists its runs without a click")
        compare(runs.title, "Scenario B")
        verify(findChild(panel, "scenarioItem_1").highlighted)
        verify(!findChild(panel, "scenarioItem_0").highlighted)
    }

    // --- Recent is bounded, whole-row and visibly scrollable ------------

    function makeRecent(count) {
        const runs = []
        for (let i = 0; i < count; ++i)
            runs.push({hash: "r" + (i % 3), scenarioName: "Scenario " + (i % 3), runLabel: "recent " + i,
                       startTimeMs: 1723200000000 - i * 60000, score: 5000 - i, accuracy: 0.7,
                       shots: 100, hits: 70, personalBest: false})
        return runs
    }

    function test_expandedRecentIsBoundedAndLeavesTheListsUsable() {
        const scenarios = []
        for (let i = 0; i < 12; ++i)
            scenarios.push({name: "Scenario " + i, hash: "s" + i, runCount: 8, lastPlayedMs: 1723200000000})
        const runs = []
        for (let i = 0; i < 30; ++i)
            runs.push({hash: "s0", scenarioName: "Scenario 0", runLabel: "run " + i,
                       startTimeMs: 1723200000000 - i * 60000, score: 3000 + i, accuracy: 0.6,
                       shots: 100, hits: 60, personalBest: false})
        // About the left column's height in a 1000x640 window.
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 330, height: 540, scenarioModel: scenarios, runModel: runs,
            recentRunModel: makeRecent(10), activeScenarioHash: "s0", activeScenarioName: "Scenario 0"
        })
        verify(waitForRendering(panel))
        const recentToggle = findChild(panel, "recentToggleButton")
        mouseClick(recentToggle, recentToggle.width / 2, recentToggle.height / 2)
        const recent = findChild(panel, "recentRunsView")
        tryVerify(() => recent.visible)
        wait(50)

        const recentList = findChild(recent, "runListView")
        verify(recent.height <= panel.height * 0.3, "Recent takes " + recent.height + "px of " + panel.height)
        compare((recentList.height + recent.cardSpacing) % recent.rowHeight, 0,
                "Recent must end on a whole card, never through a run's text")
        verify((recentList.height + recent.cardSpacing) / recent.rowHeight >= 2)
        verify(recent.scrollable)
        verify(findChild(recent, "runListScrollBar").visible, "Recent must visibly indicate that it scrolls")

        const scenarioList = findChild(panel, "scenarioListView")
        const scenarioRow = findChild(panel, "scenarioItem_0")
        verify(scenarioList.height >= 3 * scenarioRow.height,
               "scenario list shows " + (scenarioList.height / scenarioRow.height).toFixed(1) + " rows")
        const runs2 = findChild(panel, "scenarioRunsView")
        const runList = findChild(runs2, "runListView")
        verify(runList.height + runs2.cardSpacing >= 2 * runs2.rowHeight,
               "run list shows " + (runList.height / runs2.rowHeight).toFixed(1) + " cards")
    }
}
