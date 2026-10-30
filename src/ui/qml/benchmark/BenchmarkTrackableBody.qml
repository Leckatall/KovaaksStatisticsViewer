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
    property var visualSettings: null
    property real bodyWidth
    property bool wide

    signal manageBenchmarksRequested()
    signal moreGraphSettingsRequested(string graphKey)

    // Explicit x/y placement rather than a Flow: the three history cards
    // stay direct children (the scroll-order test walks them by index) yet
    // their side-by-side / stacked geometry resolves without a layout
    // polish, which the responsive tests rely on. Wide, the two rank cards
    // share a row and playtime spans the one below.
    //
    // A hidden card keeps its place in that arithmetic: it collapses to no height, or to the notice's
    // height while the quick-hide notice stands in its slot, and a slot in use claims the same space
    // whichever of the two fills it.
    Item {
        id: trackableScrollColumn
        objectName: "trackableScrollColumn"
        width: benchmarkWorkspaceScroll.bodyWidth
        implicitWidth: width
        implicitHeight: recovery.visible ? recovery.y + recovery.height : playtimeCard.y + playtimeCard.height

        readonly property real gap: 12
        readonly property real halfWidth: Math.floor((width - gap) / 2)

        readonly property var settings: benchmarkWorkspaceScroll.visualSettings
        readonly property string noticeKey: settings && settings.hideNoticeGraph ? settings.hideNoticeGraph : ""
        readonly property bool rankShown: !settings || settings.benchmarkRankGraphVisible
        readonly property bool recentRankShown: !settings || settings.benchmarkRecentRankGraphVisible
        readonly property bool playtimeShown: !settings || settings.benchmarkPlaytimeGraphVisible
        readonly property bool rankSlot: rankShown || noticeKey === "benchmarkRank"
        readonly property bool recentRankSlot: recentRankShown || noticeKey === "benchmarkRecentRank"
        readonly property bool playtimeSlot: playtimeShown || noticeKey === "benchmarkPlaytime"
        readonly property bool rankRow: benchmarkWorkspaceScroll.wide && rankSlot && recentRankSlot
        readonly property bool anyRankSlot: rankSlot || recentRankSlot
        readonly property real rankRowBottom: Math.max(rankCard.y + rankCard.height,
                                                       recentRankCard.y + recentRankCard.height)

        function slotHeight(card, shown, slot) {
            return shown ? Math.max(card.implicitHeight, 220) : (slot ? notice.implicitHeight : 0)
        }

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
            width: trackableScrollColumn.rankRow ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
            height: trackableScrollColumn.slotHeight(rankCard, trackableScrollColumn.rankShown,
                                                     trackableScrollColumn.rankSlot)
            visible: trackableScrollColumn.rankShown
            namePrefix: "rankHistory"
            graphKey: "benchmarkRank"
            settingsVm: benchmarkWorkspaceScroll.settingsVm
            onMoreGraphSettingsRequested: key => benchmarkWorkspaceScroll.moreGraphSettingsRequested(key)
            onHideGraphRequested: key => trackableScrollColumn.settings.quickHideGraph(key)
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.rankHistory : null
        }

        BenchmarkHistoryCard {
            id: recentRankCard
            objectName: "recentRankHistoryCard"
            x: trackableScrollColumn.rankRow ? (trackableScrollColumn.halfWidth + trackableScrollColumn.gap) : 0
            y: trackableScrollColumn.rankRow || !trackableScrollColumn.rankSlot
               ? rankCard.y : (rankCard.y + rankCard.height + trackableScrollColumn.gap)
            width: trackableScrollColumn.rankRow ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
            height: trackableScrollColumn.slotHeight(recentRankCard, trackableScrollColumn.recentRankShown,
                                                     trackableScrollColumn.recentRankSlot)
            visible: trackableScrollColumn.recentRankShown
            namePrefix: "recentRankHistory"
            graphKey: "benchmarkRecentRank"
            settingsVm: benchmarkWorkspaceScroll.settingsVm
            onMoreGraphSettingsRequested: key => benchmarkWorkspaceScroll.moreGraphSettingsRequested(key)
            onHideGraphRequested: key => trackableScrollColumn.settings.quickHideGraph(key)
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.recentRankHistory : null
        }

        BenchmarkHistoryCard {
            id: playtimeCard
            objectName: "playtimeHistoryCard"
            x: 0
            y: trackableScrollColumn.rankRowBottom
               + (trackableScrollColumn.anyRankSlot && trackableScrollColumn.playtimeSlot ? trackableScrollColumn.gap : 0)
            width: trackableScrollColumn.width
            height: trackableScrollColumn.slotHeight(playtimeCard, trackableScrollColumn.playtimeShown,
                                                     trackableScrollColumn.playtimeSlot)
            visible: trackableScrollColumn.playtimeShown
            namePrefix: "playtimeHistory"
            graphKey: "benchmarkPlaytime"
            settingsVm: benchmarkWorkspaceScroll.settingsVm
            onMoreGraphSettingsRequested: key => benchmarkWorkspaceScroll.moreGraphSettingsRequested(key)
            onHideGraphRequested: key => trackableScrollColumn.settings.quickHideGraph(key)
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.playtimeHistory : null
        }

        GraphHiddenNotice {
            id: notice
            readonly property Item slotCard: ({
                benchmarkRank: rankCard,
                benchmarkRecentRank: recentRankCard,
                benchmarkPlaytime: playtimeCard
            })[trackableScrollColumn.noticeKey] || null
            graphKey: slotCard ? trackableScrollColumn.noticeKey : ""
            visualSettings: trackableScrollColumn.settings
            x: slotCard ? slotCard.x : 0
            y: slotCard ? slotCard.y : 0
            width: slotCard ? slotCard.width : 0
        }

        NoGraphsShown {
            id: recovery
            visible: !trackableScrollColumn.rankShown && !trackableScrollColumn.recentRankShown
                     && !trackableScrollColumn.playtimeShown
            y: playtimeCard.y + playtimeCard.height
               + (trackableScrollColumn.anyRankSlot || trackableScrollColumn.playtimeSlot
                  ? trackableScrollColumn.gap : 0)
            width: trackableScrollColumn.width
            message: qsTr("Your benchmark graphs are hidden. Choose which graphs to show.")
            visualSettings: trackableScrollColumn.settings
        }
    }
}
