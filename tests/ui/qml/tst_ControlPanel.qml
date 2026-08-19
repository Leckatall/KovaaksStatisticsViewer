import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "ControlPanelTest"
    when: windowShown

    property var fakeGraphVm: QtObject {
        property var allSeries: [
            {"id": "1", "name": "Score", "color": "#009600"},
            {"id": "2", "name": "Accuracy", "color": "#00ffff"}
        ]
        property var enabledSeriesIds: ["1", "2"]
    }
    property var visualSettings: QtObject {
        property var visibleSeriesIds: []
        function isSeriesVisible(id) { return visibleSeriesIds.indexOf(id) !== -1 }
        function setSeriesVisible(id, visible) {
            const index = visibleSeriesIds.indexOf(id)
            if (visible && index === -1) visibleSeriesIds = visibleSeriesIds.concat([id])
            else if (!visible && index !== -1) {
                const next = visibleSeriesIds.slice()
                next.splice(index, 1)
                visibleSeriesIds = next
            }
        }
        function syncVisibleSeriesIds(ids) { visibleSeriesIds = ids }
    }

    Component.onCompleted: visualSettings.syncVisibleSeriesIds(fakeGraphVm.enabledSeriesIds)

    ControlPanel {
        id: panel
        graphVm: testCase.fakeGraphVm
        visualSettings: testCase.visualSettings
    }

    function test_checkbox_starts_checked_for_visible_series() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_1")
        verify(checkbox !== null)
        compare(checkbox.checked, true)
    }

    function test_unchecking_then_rechecking_round_trips_visibility() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_1")
        checkbox.checked = false
        checkbox.toggled()
        compare(testCase.visualSettings.isSeriesVisible("1"), false)
        checkbox.checked = true
        checkbox.toggled()
        compare(testCase.visualSettings.isSeriesVisible("1"), true)
    }
}
