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
        return [{hash: hash, runLabel: "run", scenarioName: "Scenario A", startTimeMs: 1723200000000,
                 score: 8421, accuracy: 0.91, durationSeconds: 60, shots: 132, hits: 120}]
    }

    function createPanel() {
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 600, height: 700,
            scenarioModel: [{name: "Scenario A", hash: "a", runCount: 3, lastPlayedMs: 1723200000000}],
            runModel: makeRuns("a"), recentRunModel: makeRuns("recent")
        })
        verify(waitForRendering(panel))
        return panel
    }

    function test_widthIsUnaffectedByScenarioAndRunSelection() {
        const longName = "A".repeat(200)
        const panel = createTemporaryObject(panelComponent, testCase, {
            height: 700, maximumPanelWidth: 400,
            scenarioModel: [{name: longName, hash: "a", runCount: 3, lastPlayedMs: 1723200000000}],
            runModel: makeRuns("a"), recentRunModel: makeRuns("recent")
        })
        verify(waitForRendering(panel))
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
        const panel = createTemporaryObject(panelComponent, testCase, {
            height: 700, maximumPanelWidth: 400, widestScenarioName: "A".repeat(200)
        })
        verify(waitForRendering(panel))
        compare(panel.desiredWidth, 400)

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
            {hash: "a", runLabel: "current", scenarioName: "Scenario A", startTimeMs: currentTime,
             score: 8421, accuracy: 0.91, durationSeconds: 60, shots: 132, hits: 120},
            {hash: "a", runLabel: "next", scenarioName: "Scenario A", startTimeMs: nextTime,
             score: 8150, accuracy: 0.885, durationSeconds: 59.8, shots: 128, hits: 113}
        ]
        const panel = createTemporaryObject(panelComponent, testCase, {
            width: 600, height: 700, currentRunHash: "a", currentRunStartTimeMs: currentTime,
            scenarioModel: [{name: "Scenario A", hash: "a", runCount: 2, lastPlayedMs: currentTime}],
            runModel: runs, recentRunModel: runs
        })
        verify(waitForRendering(panel))

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
}
