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
    property real bodyWidth
    property bool wide

    signal manageBenchmarksRequested()

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

        BenchmarkStatusSummary {
            id: summaryBlock
            objectName: "benchmarkStatusSummary"
            x: 0
            y: 0
            width: trackableScrollColumn.width
            trackingVm: benchmarkWorkspaceScroll.trackingVm
        }

        ColumnLayout {
            id: progressBlock
            x: 0
            y: summaryBlock.y + summaryBlock.height + trackableScrollColumn.gap
            width: trackableScrollColumn.width
            spacing: 4

            Label {
                objectName: "nextTierProgress"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                textFormat: Text.StyledText
                visible: text.length > 0
                text: {
                    const vm = benchmarkWorkspaceScroll.trackingVm
                    if (!vm || !vm.summaryAvailable) return ""
                    if (!vm.hasNextTier) return qsTr("Highest tier attained")
                    const count = vm.nextTierBlockerCount
                    const color = String(vm.nextTierColor || "")
                    const tier = color.length === 0 ? String(vm.nextTier)
                        : "<font color=\"" + color + "\">"
                          + String(vm.nextTier).replace(/&/g, "&amp;")
                                .replace(/</g, "&lt;").replace(/>/g, "&gt;")
                          + "</font>"
                    // An empty attained colour means Unranked. Ranks are attained per group, so this counts the rows marked as blockers,
                    // not the scenarios already at the next tier.
                    let line
                    if (count === 0) line = qsTr("No scenario blocks %1").arg(tier)
                    else if (count === 1) line = qsTr("1 scenario blocks %1").arg(tier)
                    else line = qsTr("%1 scenarios block %2").arg(count).arg(tier)
                    return String(vm.attainedRankColor || "") === ""
                           ? qsTr("Not ranked yet: %1").arg(line) : line
                }
            }
            // Every scenario with its tier bar; rows that block the next tier
            // are marked, so the count above has its reasons right below it.
            BenchmarkProgressGrid {
                objectName: "benchmarkProgressGrid"
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                Layout.topMargin: 4
                grid: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.progressGrid : null
                benchmarkKey: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.selectedName : ""
            }
        }

        BenchmarkHistoryCard {
            id: rankCard
            objectName: "rankHistoryCard"
            x: 0
            y: progressBlock.y + progressBlock.height + trackableScrollColumn.gap
            width: benchmarkWorkspaceScroll.wide ? trackableScrollColumn.halfWidth : trackableScrollColumn.width
            height: Math.max(implicitHeight, 220)
            namePrefix: "rankHistory"
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
            historyModel: benchmarkWorkspaceScroll.trackingVm ? benchmarkWorkspaceScroll.trackingVm.playtimeHistory : null
        }
    }
}
