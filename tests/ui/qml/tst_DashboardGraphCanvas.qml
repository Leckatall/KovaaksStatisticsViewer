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
        property var isSeriesVisible: function (id) { return true }
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
