import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"

Item {
    id: root

    width: 600
    height: 400

    Component {
        id: tooltipComponent

        GraphCanvasWithTooltip {
            width: 600
            height: 400
            graphVm: null
        }
    }

    TestCase {
        name: "GraphCanvasWithTooltipTest"
        when: windowShown

        function hoverOver(graph) {
            findChild(graph, "graphHoverArea").hoverInfo = {valid: true, x: "Mar 4, 14:32", xRaw: 7, series: []}
        }

        function test_detailRowShowsTheTextForTheHoveredX() {
            const graph = createTemporaryObject(tooltipComponent, root,
                                                {xLabel: "Date", hoverDetail: x => "Run: #" + x})
            hoverOver(graph)

            const detail = findChild(graph, "graphHoverDetail")
            verify(detail.visible, "the detail row shows under the header")
            compare(detail.text, "Run: #7")
            compare(findChild(graph, "graphHoverHeader").text, "Date: Mar 4, 14:32")
        }

        function test_noDetailRowWithoutAHoverDetail() {
            const graph = createTemporaryObject(tooltipComponent, root)
            hoverOver(graph)

            verify(findChild(graph, "graphHoverHeader").visible, "the tooltip is showing")
            verify(!findChild(graph, "graphHoverDetail").visible, "no detail row without a hoverDetail")
        }
    }
}
