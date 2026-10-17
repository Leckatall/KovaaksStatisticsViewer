import QtQuick
import QtQuick.Controls
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"
import "TestDoubles.js" as TestDoubles

Item {
    id: root

    width: 900
    height: 600

    QtObject {
        id: historyVisibilitySettings

        property var value: ({"1": true, "2": false, "3": false, "4": false, "5": false, "6": false})

        function request(id, shown) {
            value = Object.assign({}, value, {[id]: shown})
        }
    }

    QtObject {
        id: axisSettings

        property string yAxisColumnKey: "1"
    }

    Component {
        id: panelComponent

        ScenarioHistoryPanel {
            width: 900
            height: 600
            historyVm: TestDoubles.makeFakeHistoryVm()
            columnVisibility: historyVisibilitySettings.value
            historyAxisSettings: axisSettings
            onSeriesVisibilityRequested: (id, shown) => historyVisibilitySettings.request(id, shown)
        }
    }

    Component {
        id: fewRunsPanelComponent

        ScenarioHistoryPanel {
            width: 900
            height: 600
            historyVm: Object.assign(TestDoubles.makeFakeHistoryVm(), {runCount: 8, visibleRunCount: 8})
            columnVisibility: historyVisibilitySettings.value
            historyAxisSettings: axisSettings
            onSeriesVisibilityRequested: (id, shown) => historyVisibilitySettings.request(id, shown)
        }
    }

    TestCase {
        name: "ScenarioHistoryPanelTest"
        when: windowShown

        function init() {
            historyVisibilitySettings.value = {"1": true, "2": false, "3": false, "4": false, "5": false, "6": false}
            axisSettings.yAxisColumnKey = "1"
        }

        function test_legendListsAllSixLinesIncludingHiddenOnes() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            for (let id = 1; id <= 6; ++id) {
                const box = findChild(panel, "seriesVisibilityCheckBox_" + id)
                verify(box !== null, "legend chip " + id)
                compare(box.checked, panel.visibleColumns.indexOf(id) !== -1)
            }
            const legend = findChild(panel, "historyLineToggles")
            verify(legend !== null)
            compare(findChild(legend, "configureGraphLinesButton").visible, false)
        }

        function test_legendTogglesRequestVisibilityAndUpdateTheColumns() {
            const panel = createTemporaryObject(panelComponent, root)
            const requests = []
            panel.seriesVisibilityRequested.connect((id, shown) => requests.push([id, shown]))
            const accuracy = findChild(panel, "seriesVisibilityCheckBox_2")
            accuracy.checked = true
            accuracy.toggled()
            const average = findChild(panel, "seriesVisibilityCheckBox_6")
            average.checked = true
            average.toggled()
            compare(requests, [["2", true], ["6", true]])
            tryCompare(panel, "visibleColumns", [1, 2, 6])
        }

        function test_legendCanHideTheAverage() {
            historyVisibilitySettings.value = {"1": true, "6": true}
            const panel = createTemporaryObject(panelComponent, root)
            compare(panel.visibleColumns, [1, 6])
            const average = findChild(panel, "seriesVisibilityCheckBox_6")
            average.checked = false
            average.toggled()
            tryCompare(panel, "visibleColumns", [1])
        }

        function test_missingAverageEntryDefaultsToVisible() {
            historyVisibilitySettings.value = {"1": false}
            const panel = createTemporaryObject(panelComponent, root)
            compare(panel.visibleColumns, [6])
            compare(findChild(panel, "seriesVisibilityCheckBox_6").checked, true)
        }

        function test_averageOnlyFallsBackToTheScoreAxisColumn() {
            historyVisibilitySettings.value = {"1": false, "6": true}
            const panel = createTemporaryObject(panelComponent, root)
            compare(panel.yAxisColumn, 6)
        }

        function test_allLinesHiddenStillShowsTheLegend() {
            historyVisibilitySettings.value = {"1": false, "2": false, "3": false, "4": false, "5": false, "6": false}
            const panel = panelWith({runCount: 8, visibleRunCount: 8})
            compare(panel.visibleColumns, [])
            compare(panel.yAxisColumn, -1)
            verify(findChild(panel, "seriesVisibilityCheckBox_6") !== null)
        }

        function test_legendRemainsWithAnEmptyRange() {
            const panel = panelWith({runCount: 5, visibleRunCount: 0})
            verify(findChild(panel, "historyLineToggles").visible)
            compare(findChild(panel, "historyEmptyLabel").visible, true)
        }

        function test_legendScrollsInAShortPanelWithoutGrowingIt() {
            const panel = createTemporaryObject(panelComponent, root, {height: 100})
            const list = findChild(panel, "graphLineChipList")
            tryVerify(() => list.contentHeight > list.height)
            compare(panel.height, 100)
            compare(findChild(panel, "graphLineChipScrollBar").policy, ScrollBar.AlwaysOn)
        }

        function test_visibleColumnsTrackSettings() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            compare(panel.visibleColumns, [1])

            historyVisibilitySettings.value = Object.assign({}, historyVisibilitySettings.value, {"2": true})
            tryCompare(panel, "visibleColumns", [1, 2])
        }

        // function test_yAxisColumnResolvesKnownAndUnknownKeys() {
        //     const panel = createTemporaryObject(panelComponent, root)
        //     verify(!!panel, "Component exists")
        //     compare(panel.yAxisColumn, 1)
        //
        //     axisSettings.yAxisColumnKey = "missing"
        //     tryCompare(panel, "yAxisColumn", -1)
        // }

        // the history graph asks its canvas to dot each real run (shown while sparse).
        function test_historyGraphMarksRealRuns() {
            const panel = createTemporaryObject(fewRunsPanelComponent, root)
            verify(!!panel, "Component exists")
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, true)
        }

        // The line style is the committed preference of this graph: markers and connection follow the VM.
        function test_markersAndConnectionFollowTheViewModel() {
            const panel = panelWith({runCount: 8, visibleRunCount: 8, pointMarkers: false, connectionMode: 2})
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, false)
            compare(canvas.connectionMode, 2)

            panel.historyVm = Object.assign(TestDoubles.makeFakeHistoryVm(),
                                            {runCount: 8, visibleRunCount: 8, pointMarkers: true, connectionMode: 1})
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, true)
            compare(canvas.connectionMode, 1)
        }

        // The canvas marks the run the VM reports as selected, and nothing without one.
        function test_selectedRunIsMarkedOnTheCanvas() {
            const panel = createTemporaryObject(fewRunsPanelComponent, root)
            verify(!!panel, "Component exists")
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            verify(isNaN(canvas.markedX), "no selected run marks nothing, got " + canvas.markedX)

            panel.historyVm = Object.assign(TestDoubles.makeFakeHistoryVm(), {runCount: 8, visibleRunCount: 8, selectedRunIndex: 6, selectedRunVisible: true})
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            tryCompare(canvas, "markedX", 6)

            panel.historyVm = Object.assign(TestDoubles.makeFakeHistoryVm(), {runCount: 8, visibleRunCount: 8, selectedRunIndex: 2, selectedRunVisible: true})
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            tryCompare(canvas, "markedX", 2)
        }

        function panelWith(vmOverrides) {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            panel.historyVm = Object.assign(TestDoubles.makeFakeHistoryVm(), vmOverrides)
            return panel
        }

        // One visible run still loads the chart (its marker and tooltip), not a refusal.
        function test_oneVisibleRunLoadsTheChart() {
            const panel = panelWith({runCount: 8, visibleRunCount: 1})
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, true)
            compare(findChild(panel, "historyEmptyLabel").visible, false)
        }

        // A selected run outside the visible set stays selected in the VM but draws no marker.
        function test_selectedRunOutsideTheVisibleSetIsNotMarked() {
            const panel = panelWith({runCount: 8, visibleRunCount: 3, selectedRunIndex: 2, selectedRunVisible: false})
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "historyGraphCanvas")) !== null, 2000)
            verify(isNaN(canvas.markedX), "an out-of-range selection marks nothing, got " + canvas.markedX)
            compare(panel.historyVm.selectedRunIndex, 2)
        }

        function test_noHistoryAndAnEmptyRangeAreToldApart() {
            const noHistory = panelWith({runCount: 0, visibleRunCount: 0})
            const emptyRange = panelWith({runCount: 5, visibleRunCount: 0})

            const noHistoryLabel = findChild(noHistory, "historyEmptyLabel")
            const emptyRangeLabel = findChild(emptyRange, "historyEmptyLabel")
            compare(noHistoryLabel.visible, true)
            compare(emptyRangeLabel.visible, true)
            verify(noHistoryLabel.text !== emptyRangeLabel.text, "same text for both: " + noHistoryLabel.text)
            compare(findChild(noHistory, "historyGraphCanvas"), null)
            compare(findChild(emptyRange, "historyGraphCanvas"), null)
        }

        function test_noSelectedScenarioIsToldApartFromAScenarioWithoutRuns() {
            const noScenario = panelWith({scenarioTitle: "", runCount: 0, visibleRunCount: 0})
            const noRuns = panelWith({scenarioTitle: "Air Angelic", runCount: 0, visibleRunCount: 0})

            compare(findChild(noScenario, "historyEmptyLabel").text, "No scenario selected.")
            compare(findChild(noRuns, "historyEmptyLabel").text, "No runs recorded for this scenario.")
        }

        function test_nothingAsksForTwoRuns() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            verify(!findChild(panel, "historyEmptyLabel").text.includes("2 runs"))
        }

        // The rotated Y-axis title names the labelled axis in the default grey, never a series colour.
        function test_yAxisTitleIsGrey() {
            const panel = createTemporaryObject(fewRunsPanelComponent, root)
            verify(!!panel, "Component exists")
            let label = null
            tryVerify(() => (label = findChild(panel, "historyYAxisTitleLabel")) !== null, 2000)
            verify(Qt.colorEqual(label.color, Qt.alpha(label.palette.text, 0.6)), "axis title colour " + label.color)
        }

        function test_titleBindsThroughHistoryViewModel() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const title = findChild(panel, "historyTitleLabel")
            verify(!!title, "Object exists")
            compare(title.text, qsTr("Air Angelic"))
        }
    }
}
