import QtQuick
import QtTest
import "../../../src/ui/qml/graph"

TestCase {
    id: testCase
    name: "VisualSettingsManagerTest"

    VisualSettingsManager {
        id: manager
    }

    // The run graph axis label belongs to the graph configuration document, which has the only writer.
    function test_run_graph_axis_label_is_not_a_visual_setting() {
        compare(manager.graphAxisSeriesId, undefined)
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

    readonly property var graphProperties: ({
        run: "scenarioGraphVisible",
        dailyPlaytime: "playtimeGraphVisible",
        scenarioHistory: "scenarioHistoryGraphVisible",
        benchmarkRank: "benchmarkRankGraphVisible",
        benchmarkRecentRank: "benchmarkRecentRankGraphVisible",
        benchmarkPlaytime: "benchmarkPlaytimeGraphVisible"
    })

    function cleanup() {
        manager.historyColumnVisibility = {"1": true, "2": false, "3": false, "4": false, "5": false, "6": true}
        for (const key of Object.keys(graphProperties)) manager.setGraphVisible(key, true)
        manager.dismissHideNotice()
    }

    function test_every_graph_key_reads_and_writes_its_own_saved_setting() {
        for (const key of Object.keys(graphProperties)) {
            compare(manager.isGraphVisible(key), true, key)
            manager.setGraphVisible(key, false)
            compare(manager[graphProperties[key]], false, key)
            compare(manager.isGraphVisible(key), false, key)
            manager.setGraphVisible(key, true)
        }
    }

    function test_quick_hide_hides_the_graph_and_raises_its_notice() {
        manager.quickHideGraph("benchmarkRank")
        compare(manager.benchmarkRankGraphVisible, false)
        compare(manager.hideNoticeGraph, "benchmarkRank")
    }

    function test_undo_restores_the_graph_and_clears_the_notice() {
        manager.quickHideGraph("run")
        manager.undoQuickHide()
        compare(manager.scenarioGraphVisible, true)
        compare(manager.hideNoticeGraph, "")
    }

    function test_dismiss_clears_the_notice_and_keeps_the_graph_hidden() {
        manager.quickHideGraph("run")
        manager.dismissHideNotice()
        compare(manager.scenarioGraphVisible, false)
        compare(manager.hideNoticeGraph, "")
    }

    function test_a_second_quick_hide_replaces_the_notice_and_leaves_the_first_graph_hidden() {
        manager.quickHideGraph("run")
        manager.quickHideGraph("dailyPlaytime")
        compare(manager.hideNoticeGraph, "dailyPlaytime")
        compare(manager.scenarioGraphVisible, false)
    }

    function test_restoring_a_graph_elsewhere_retires_its_notice_for_good() {
        manager.quickHideGraph("run")
        manager.scenarioGraphVisible = true
        compare(manager.hideNoticeGraph, "")
        manager.scenarioGraphVisible = false
        compare(manager.hideNoticeGraph, "")
    }

    function test_restoring_another_graph_keeps_the_notice() {
        manager.playtimeGraphVisible = false
        manager.quickHideGraph("run")
        manager.playtimeGraphVisible = true
        compare(manager.hideNoticeGraph, "run")
    }

    function test_the_notice_does_not_survive_a_restart_but_the_hidden_graph_does() {
        const before = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
            testCase, "noticeBeforeRestartManager")
        before.quickHideGraph("benchmarkPlaytime")
        before.destroy()
        wait(50)
        const after = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
            testCase, "noticeAfterRestartManager")
        compare(after.benchmarkPlaytimeGraphVisible, false)
        compare(after.hideNoticeGraph, "")
        after.benchmarkPlaytimeGraphVisible = true
        after.destroy()
        wait(50)
    }

    function test_historyVisibilityPreservesOldChoicesAndDefaultsTheNewAverageOn() {
        manager.historyColumnVisibility = {"1": false, "2": true}
        compare(manager.isHistorySeriesVisible("1"), false)
        compare(manager.isHistorySeriesVisible("2"), true)
        compare(manager.isHistorySeriesVisible("6"), true)
        compare(manager.isHistorySeriesVisible("3"), false)
        manager.setHistorySeriesVisible("6", false)
        compare(manager.isHistorySeriesVisible("6"), false)
        compare(manager.historyColumnVisibility["2"], true)
    }

    function test_hidden_history_average_stays_hidden_after_restart() {
        const before = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
            testCase, "historyBeforeRestartManager")
        before.setHistorySeriesVisible("6", false)
        before.destroy()
        wait(50)
        const after = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
            testCase, "historyRestartManager")
        wait(50)
        compare(after.isHistorySeriesVisible("6"), false)
        after.destroy()
    }

    function test_hidden_series_stays_hidden_after_restart() {
        // Simulated app restart: a fresh instance reading back whatever the previous
        // instance persisted, rather than reusing the shared `manager` from above.
        const before = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
            testCase, "beforeRestartManager")
        before.syncVisibleSeriesIds(["10", "11"])
        before.setSeriesVisible("10", false)
        before.destroy()
        wait(50) // let the destroyed instance's Settings flush before reading it back

        const after = Qt.createQmlObject(
            'import "../../../src/ui/qml/graph"; VisualSettingsManager {}',
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
