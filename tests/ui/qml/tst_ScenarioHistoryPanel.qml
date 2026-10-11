import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml"
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
            historyVm: Object.assign(TestDoubles.makeFakeHistoryVm(), {runCount: 8})
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
