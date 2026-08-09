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

    function test_activationRevealsScenarioRunsAndRecentRenders() {
        const panel = createPanel()
        const scenarioRuns = findChild(panel, "scenarioRunsView")
        const recentRuns = findChild(panel, "recentRunsView")
        verify(findChild(recentRuns, "runItem_0") !== null)
        compare(scenarioRuns.visible, false)

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
        const recentItem = findChild(findChild(panel, "recentRunsView"), "runItem_0")
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
}
