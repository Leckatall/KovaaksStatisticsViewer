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

        property bool score: true
        property bool accuracy: false
        property bool shots: false
        property bool hits: false
        property bool misses: false
    }

    QtObject {
        id: axisSettings

        property string yAxisColumnKey: "score"
    }

    Component {
        id: panelComponent

        ScenarioHistoryPanel {
            width: 900
            height: 600
            historyVm: TestDoubles.makeFakeHistoryVm()
            columnVisibility: historyVisibilitySettings
            historyAxisSettings: axisSettings
        }
    }

    TestCase {
        name: "ScenarioHistoryPanelTest"
        when: windowShown

        function init() {
            historyVisibilitySettings.score = true
            historyVisibilitySettings.accuracy = false
            historyVisibilitySettings.shots = false
            historyVisibilitySettings.hits = false
            historyVisibilitySettings.misses = false
            axisSettings.yAxisColumnKey = "score"
        }

        function test_visibleColumnsTrackSettings() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            compare(panel.visibleColumns, [1])

            historyVisibilitySettings.accuracy = true
            tryCompare(panel, "visibleColumns", [1, 2])
        }

        function test_yAxisColumnResolvesKnownAndUnknownKeys() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            compare(panel.yAxisColumn, 1)

            axisSettings.yAxisColumnKey = "missing"
            tryCompare(panel, "yAxisColumn", -1)
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
