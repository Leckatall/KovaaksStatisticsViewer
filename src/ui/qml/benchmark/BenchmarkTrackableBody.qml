import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// The bar is always visible while the content overflows: the history cards usually
// starts below the fold, and the style's own bar only shows mid-scroll.
OverflowScrollView {
    id: benchmarkWorkspaceScroll
    objectName: "benchmarkWorkspaceScroll"

    property var trackingVm
    property var settingsVm: null
    property real bodyWidth
    property bool wide

    signal manageBenchmarksRequested()
    signal moreGraphSettingsRequested(string graphKey)

    // Explicit x/y placement rather than a Flow: the two history cards
    // stay direct children (the scroll-order test walks them by index) yet
    // their side-by-side / stacked geometry resolves without a layout
    // polish, which the responsive tests rely on.
    Item {
        id: trackableScrollColumn
        objectName: "trackableScrollColumn"
        width: benchmarkWorkspaceScroll.bodyWidth
        implicitWidth: width
        implicitHeight: benchmarkWorkspaceScroll.wide ? rankCard.y + Math.max(rankCard.height, playtimeCard.height)
                                                      : playtimeCard.y + playtimeCard.height

        readonly property real gap: 12
        readonly property real halfWidth: Math.floor((width - gap) / 2)

        // Every scenario with its tier bar; rows that block the next tier are marked.
        BenchmarkProgressGrid {
            id: progressBlock
            objectName: "benchmarkProgressGrid"
            x: 0
            y: 0
            width: trackableScrollColumn.width
            height: implicitHeight
            grid: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.progressGrid : null
            benchmarkKey: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.selectedName : ""
        }

        BenchmarkHistoryCard {
            id: rankCard
            objectName: "rankHistoryCard"
            x: 0
            y: progressBlock.y + progressBlock.height + trackableScrollColumn.gap
            width: benchmarkWorkspaceScroll.wide ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
            height: Math.max(implicitHeight, 220)
            namePrefix: "rankHistory"
            graphKey: "benchmarkRank"
            settingsVm: benchmarkWorkspaceScroll.settingsVm
            onMoreGraphSettingsRequested: key => benchmarkWorkspaceScroll.moreGraphSettingsRequested(key)
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.rankHistory : null
        }

        BenchmarkHistoryCard {
            id: playtimeCard
            objectName: "playtimeHistoryCard"
            x: benchmarkWorkspaceScroll.wide ? (trackableScrollColumn.halfWidth + trackableScrollColumn.gap) : 0
            y: benchmarkWorkspaceScroll.wide ? rankCard.y
                                             : (rankCard.y + rankCard.height + trackableScrollColumn.gap)
            width: benchmarkWorkspaceScroll.wide ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
            height: Math.max(implicitHeight, 220)
            namePrefix: "playtimeHistory"
            graphKey: "benchmarkPlaytime"
            settingsVm: benchmarkWorkspaceScroll.settingsVm
            onMoreGraphSettingsRequested: key => benchmarkWorkspaceScroll.moreGraphSettingsRequested(key)
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.playtimeHistory : null
        }
    }
}
