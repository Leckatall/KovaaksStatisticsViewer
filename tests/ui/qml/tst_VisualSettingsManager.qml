import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    name: "VisualSettingsManagerTest"

    VisualSettingsManager {
        id: manager
    }

    function test_newly_enabled_series_default_visible() {
        manager.syncVisibleSeriesIds(["1", "2"])
        compare(manager.isSeriesVisible("1"), true)
        compare(manager.isSeriesVisible("2"), true)
    }

    function test_toggling_visibility_is_independent_per_series() {
        manager.syncVisibleSeriesIds(["1", "2"])
        manager.setSeriesVisible("1", false)
        compare(manager.isSeriesVisible("1"), false)
        compare(manager.isSeriesVisible("2"), true)
    }

    function test_resyncing_forgets_ids_no_longer_enabled_and_keeps_the_rest() {
        manager.syncVisibleSeriesIds(["1", "2"])
        manager.setSeriesVisible("1", false)
        manager.syncVisibleSeriesIds(["2", "3"])
        compare(manager.isSeriesVisible("2"), true)
        compare(manager.isSeriesVisible("3"), true)
    }
}
