import QtQuick

// VisualSettingsManager's graph visibility without its saved settings, which outlive a test run.
QtObject {
    property bool scenarioGraphVisible: true
    property bool playtimeGraphVisible: true
    property bool scenarioHistoryGraphVisible: true
    property bool benchmarkRankGraphVisible: true
    property bool benchmarkRecentRankGraphVisible: true
    property bool benchmarkPlaytimeGraphVisible: true
    property string hideNoticeGraph: ""

    readonly property var graphVisibilityProperties: ({
        run: "scenarioGraphVisible",
        dailyPlaytime: "playtimeGraphVisible",
        scenarioHistory: "scenarioHistoryGraphVisible",
        benchmarkRank: "benchmarkRankGraphVisible",
        benchmarkRecentRank: "benchmarkRecentRankGraphVisible",
        benchmarkPlaytime: "benchmarkPlaytimeGraphVisible"
    })

    function isGraphVisible(graphKey) { return this[graphVisibilityProperties[graphKey]] }
    function setGraphVisible(graphKey, shown) { this[graphVisibilityProperties[graphKey]] = shown }
    function quickHideGraph(graphKey) {
        setGraphVisible(graphKey, false)
        hideNoticeGraph = graphKey
    }
    function undoQuickHide() {
        setGraphVisible(hideNoticeGraph, true)
        hideNoticeGraph = ""
    }
    function dismissHideNotice() { hideNoticeGraph = "" }
}
