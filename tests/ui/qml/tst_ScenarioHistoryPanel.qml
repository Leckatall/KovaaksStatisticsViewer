import QtQuick
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

        property var value: ({"1": true, "2": false, "3": false, "4": false, "5": false})
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
        }
    }

    TestCase {
        name: "ScenarioHistoryPanelTest"
        when: windowShown

        function init() {
            historyVisibilitySettings.value = {"1": true, "2": false, "3": false, "4": false, "5": false}
            axisSettings.yAxisColumnKey = "1"
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
