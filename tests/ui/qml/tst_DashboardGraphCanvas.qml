import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"

Item {
    id: root

    width: 900
    height: 600

    QtObject {
        id: fakeGraphVm

        property int contentState: GraphViewModel.HasData
        property string scenarioTitle: "Air Angelic"
        property var enabledSeriesIds: []
        property var allSeries: []
        property string axisLabelSeriesId: ""
        property bool pointMarkers: false
        property int connectionMode: 0
        property var columnForSeriesId: function (id) { return -1 }
        property var seriesIdForColumn: function (column) { return "" }
    }

    QtObject {
        id: fakeVisualSettings

        property bool controlPanelVisible: true
        property var isSeriesVisible: function (id) { return true }
        property var setSeriesVisible: function (id, visible) {}
    }

    SignalSpy {
        id: configureSpy
        signalName: "configureLinesRequested"
    }

    Component {
        id: panelComponent

        DashboardGraphCanvas {
            width: 900
            height: 600
            graphVm: fakeGraphVm
            visualSettings: fakeVisualSettings
        }
    }

    QtObject {
        id: manyLinesGraphVm

        property int contentState: GraphViewModel.NoRunSelected
        property string scenarioTitle: ""
        property var enabledSeriesIds: ["1", "2", "3", "4", "5", "6", "7", "8"]
        property var allSeries: [
            {"id": "1", "name": "Score", "color": "#009600", "column": 1},
            {"id": "2", "name": "Accuracy", "color": "#00ffff", "column": 2},
            {"id": "3", "name": "Shots", "color": "#ffa500", "column": 3},
            {"id": "4", "name": "Hits", "color": "#6495ed", "column": 4},
            {"id": "5", "name": "Kills", "color": "#ff0000", "column": 5},
            {"id": "6", "name": "Dmg", "color": "#ffff00", "column": 6},
            {"id": "7", "name": "Score Total", "color": "#800080", "column": 7},
            {"id": "8", "name": "Expected Final Score", "color": "#ff00ff", "column": 8}
        ]
        property string axisLabelSeriesId: ""
        property var columnForSeriesId: function (id) { return Number(id) }
        property var seriesIdForColumn: function (column) { return String(column) }
    }

    Component {
        id: manyLinesPanelComponent

        DashboardGraphCanvas {
            width: 900
            height: 160
            graphVm: manyLinesGraphVm
            visualSettings: fakeVisualSettings
        }
    }

    TestCase {
        name: "DashboardGraphCanvasTest"
        when: windowShown

        function init() {
            fakeGraphVm.contentState = GraphViewModel.HasData
            fakeGraphVm.scenarioTitle = "Air Angelic"
            fakeGraphVm.pointMarkers = false
            fakeGraphVm.connectionMode = 0
            fakeVisualSettings.controlPanelVisible = true
            manyLinesGraphVm.axisLabelSeriesId = ""
            configureSpy.clear()
        }

        function isAncestor(ancestor, item) {
            for (let p = item; p; p = p.parent) {
                if (p === ancestor) return true
            }
            return false
        }

        // The run graph draws with its own committed connection mode and marker preference.
        function test_canvasFollowsTheViewModelsLineStyle() {
            fakeGraphVm.pointMarkers = true
            fakeGraphVm.connectionMode = 2
            const panel = createTemporaryObject(panelComponent, root)
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "scenarioGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, true)
            compare(canvas.connectionMode, 2)

            fakeGraphVm.connectionMode = 1
            compare(canvas.connectionMode, 1)
        }

        // an unselected run has an empty label; the title must not invent a date for it.
        function test_noRunTitleCarriesNoDate() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            fakeGraphVm.scenarioTitle = ""

            const title = findChild(panel, "scenarioTitleLabel")
            verify(title.text.length > 0, "the panel keeps a title")
            verify(!/\d{4}-\d{2}-\d{2}/.test(title.text), "title shows a date: " + title.text)
            verify(!/\d{2}:\d{2}/.test(title.text), "title shows a time: " + title.text)
        }

        // the line toggles live inside the per-run graph panel they control.
        function test_lineTogglesSitInsideThePanel() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const toggles = findChild(panel, "graphLineToggles")
            verify(!!toggles, "toggles exist inside the panel")
            verify(toggles.visible)
            verify(isAncestor(panel, findChild(panel, "configureGraphLinesButton")))
        }

        // The legend is a column down the panel's left side, beside the title as well as the plot.
        function test_lineTogglesFormALeftColumnBesideTitleAndPlot() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const toggles = findChild(panel, "graphLineToggles")
            const content = findChild(panel, "graphContentArea")
            const title = findChild(panel, "scenarioTitleLabel")
            const togglesRight = toggles.mapToItem(panel, toggles.width, 0).x
            verify(togglesRight <= content.mapToItem(panel, 0, 0).x, "toggles sit left of the plot")
            verify(togglesRight <= title.mapToItem(panel, 0, 0).x, "toggles sit left of the title")
            const togglesTop = toggles.mapToItem(panel, 0, 0).y
            verify(togglesTop <= title.mapToItem(panel, 0, 0).y + 0.5, "the column starts beside the title")
            const contentBottom = content.mapToItem(panel, 0, content.height).y
            verify(Math.abs(toggles.mapToItem(panel, 0, toggles.height).y - contentBottom) <= 0.5,
                   "the column runs to the bottom of the plot")
        }

        // The title uses the shared chart title style (ChartTitle) and starts where the plot column
        // does, not over the legend.
        function test_titleIsLeftAlignedOverThePlot() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const title = findChild(panel, "scenarioTitleLabel")
            const content = findChild(panel, "graphContentArea")
            compare(title.horizontalAlignment, Text.AlignLeft)
            compare(title.font.weight, Font.DemiBold)
            compare(title.font.pixelSize, 14)
            fuzzyCompare(title.mapToItem(panel, 0, 0).x, content.mapToItem(panel, 0, 0).x, 0.5)
        }

        // Hiding the legend from the View menu gives the plot the full width.
        function test_hiddenLegendGivesThePlotTheFullWidth() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const content = findChild(panel, "graphContentArea")
            const withLegend = content.width
            fakeVisualSettings.controlPanelVisible = false
            tryVerify(() => content.width > withLegend)
            fuzzyCompare(content.width, panel.availableWidth, 0.5)
        }

        // The legend must not decide the panel's height, even with many lines.
        function test_legendDoesNotDecideThePanelHeight() {
            const panel = createTemporaryObject(manyLinesPanelComponent, root)
            verify(!!panel, "Component exists")
            const chip = findChild(panel, "seriesVisibilityCheckBox_1")
            verify(!!chip)
            verify(panel.implicitHeight < 8 * chip.height,
                   "panel implicit height " + panel.implicitHeight + " follows the chips")
            const button = findChild(panel, "configureGraphLinesButton")
            verify(button.mapToItem(panel, 0, button.height).y <= panel.height, "Configure... fits in the panel")
        }

        // No caption under the title; the rotated axis title alone names the axis, in the default grey.
        function test_noYAxisCaptionAndGreyAxisTitle() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            compare(findChild(panel, "yAxisScaleCaption"), null)
            let label = null
            tryVerify(() => (label = findChild(panel, "scenarioYAxisTitleLabel")) !== null)
            verify(Qt.colorEqual(label.color, Qt.alpha(label.palette.text, 0.6)), "axis title colour " + label.color)
        }

        // The axis label comes from the committed configuration the graph VM reads, not from QML settings.
        function test_yAxisColumnFollowsTheCommittedAxisLabel() {
            const panel = createTemporaryObject(manyLinesPanelComponent, root)
            verify(!!panel, "Component exists")
            compare(panel.yAxisColumn, 1)

            manyLinesGraphVm.axisLabelSeriesId = "3"
            compare(panel.yAxisColumn, 3)

            manyLinesGraphVm.axisLabelSeriesId = "42"
            compare(panel.yAxisColumn, 1, "a label naming no visible series falls back to the first visible one")
        }

        function test_configureButtonIsForwardedByThePanel() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            configureSpy.target = panel
            findChild(panel, "configureGraphLinesButton").clicked()
            compare(configureSpy.count, 1)
        }

        function test_hasDataHidesBothMessages() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeGraphVm.contentState = GraphViewModel.HasData

            const noRun = findChild(panel, "graphNoRunSelectedLabel")
            const noPerf = findChild(panel, "graphNoPerformanceLabel")
            tryCompare(noRun, "visible", false)
            compare(noPerf.visible, false)
        }

        function test_contentAreaHeightIsSameAcrossStates() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const content = findChild(panel, "graphContentArea")
            verify(!!content, "Content area exists")

            fakeGraphVm.contentState = GraphViewModel.HasData
            const withData = content.height
            verify(withData > 0, "Content area has a height")

            fakeGraphVm.contentState = GraphViewModel.NoPerformanceData
            compare(content.height, withData)

            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            compare(content.height, withData)
        }

        function test_titleStaysVisibleAcrossStates() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected

            const title = findChild(panel, "scenarioTitleLabel")
            verify(!!title, "Title exists")
            compare(title.text, "Air Angelic")
        }
    }
}
