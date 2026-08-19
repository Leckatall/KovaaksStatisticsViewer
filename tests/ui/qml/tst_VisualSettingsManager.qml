import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
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

    function test_hidden_series_stays_hidden_after_restart() {
        // Simulated app restart: a fresh instance reading back whatever the previous
        // instance persisted, rather than reusing the shared `manager` from above.
        const before = Qt.createQmlObject(
            'import "../../../src/ui/qml"; VisualSettingsManager {}',
            testCase, "beforeRestartManager")
        before.syncVisibleSeriesIds(["10", "11"])
        before.setSeriesVisible("10", false)
        before.destroy()
        wait(50) // let the destroyed instance's Settings flush before reading it back

        const after = Qt.createQmlObject(
            'import "../../../src/ui/qml"; VisualSettingsManager {}',
            testCase, "afterRestartManager")
        wait(50)
        // A fresh instance re-running the startup sync must see "10" as already-seen
        // (persisted seenSeriesIds), not as newly-enabled-and-therefore-visible.
        after.syncVisibleSeriesIds(["10", "11"])

        compare(after.isSeriesVisible("10"), false)
        compare(after.isSeriesVisible("11"), true)

        after.destroy()
    }
}
