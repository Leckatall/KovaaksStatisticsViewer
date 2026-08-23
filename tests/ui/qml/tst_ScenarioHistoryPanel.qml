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

        function test_titleBindsThroughHistoryViewModel() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const title = findChild(panel, "historyTitleLabel")
            verify(!!title, "Object exists")
            compare(title.text, qsTr("Air Angelic"))
        }
    }
}
