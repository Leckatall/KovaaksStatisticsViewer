import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml"

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
        property var columnForSeriesId: function (id) { return -1 }
        property var seriesIdForColumn: function (column) { return "" }
    }

    QtObject {
        id: fakeVisualSettings

        property int graphAxisSeriesId: 0
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

    TestCase {
        name: "DashboardGraphCanvasTest"
        when: windowShown

        function init() {
            fakeGraphVm.contentState = GraphViewModel.HasData
            fakeGraphVm.scenarioTitle = "Air Angelic"
            fakeVisualSettings.controlPanelVisible = true
            configureSpy.clear()
        }

        function isAncestor(ancestor, item) {
            for (let p = item; p; p = p.parent) {
                if (p === ancestor) return true
            }
            return false
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
            const content = findChild(panel, "graphContentArea")
            verify(toggles.y < content.y, "toggles sit above the plot")
        }

        function test_configureButtonIsForwardedByThePanel() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            configureSpy.target = panel
            findChild(panel, "configureGraphLinesButton").clicked()
            compare(configureSpy.count, 1)
        }

        function test_lineTogglesFollowTheViewSetting() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeVisualSettings.controlPanelVisible = false
            tryCompare(findChild(panel, "graphLineToggles"), "visible", false)
        }

        function test_noRunSelectedShowsOnlyItsMessage() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected

            const noRun = findChild(panel, "graphNoRunSelectedLabel")
            const noPerf = findChild(panel, "graphNoPerformanceLabel")
            tryCompare(noRun, "visible", true)
            compare(noPerf.visible, false)
        }

        function test_csvOnlyRunShowsPerformanceMessage() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            fakeGraphVm.contentState = GraphViewModel.NoPerformanceData

            const noRun = findChild(panel, "graphNoRunSelectedLabel")
            const noPerf = findChild(panel, "graphNoPerformanceLabel")
            tryCompare(noPerf, "visible", true)
            compare(noRun.visible, false)
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
