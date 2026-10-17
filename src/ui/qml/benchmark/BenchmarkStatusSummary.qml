import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// The card around it belongs to the workspace's pinned header.
Item {
    id: root

    property var trackingVm

    implicitWidth: summaryGrid.implicitWidth
    implicitHeight: summaryGrid.implicitHeight

    component Field: RowLayout {
        id: fieldRow
        property string caption
        property string value
        property string valueObjectName
        // Normal-weight text after the value, e.g. "(blocked by 2 scenarios)".
        property string suffix: ""
        property string suffixObjectName: ""
        // A tier's readable colour ("#rrggbb"), or "" for values that are not a tier.
        property string valueColor: ""
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: fieldRow.caption
            color: root.palette.placeholderText
            Layout.preferredWidth: 120
        }
        Label {
            objectName: fieldRow.valueObjectName
            text: fieldRow.value
            font.bold: true
            color: fieldRow.valueColor.length > 0 ? fieldRow.valueColor : root.palette.text
            Accessible.description: fieldRow.caption + ": " + fieldRow.value
                                    + (fieldRow.suffix.length > 0 ? " " + fieldRow.suffix : "")
        }
        Label {
            objectName: fieldRow.suffixObjectName
            visible: fieldRow.suffix.length > 0
            text: fieldRow.suffix
            color: root.palette.placeholderText
        }
    }

    function _color(name) {
        if (!root.trackingVm) return ""
        var value = root.trackingVm[name]
        return value === undefined || value === null ? "" : String(value)
    }

    // Ranks are attained per group, so this counts the rows marked as blockers, not the
    // scenarios already at the next tier.
    function _blockerSuffix() {
        const vm = root.trackingVm
        if (!vm || !vm.hasNextTier) return ""
        const count = vm.nextTierBlockerCount
        return count === 1 ? qsTr("(blocked by 1 scenario)") : qsTr("(blocked by %1 scenarios)").arg(count)
    }

    GridLayout {
        id: summaryGrid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        columns: 2
        columnSpacing: 16
        rowSpacing: 6
        // Two equal halves whatever a field needs, so the wide average-rank bar never pushes
        // the right-hand fields across the card.
        uniformCellWidths: true

        Field {
            caption: qsTr("Attained rank")
            value: root.trackingVm ? root.trackingVm.attainedRank : ""
            valueColor: root._color("attainedRankColor")
            valueObjectName: "summaryAttainedRank"
        }
        Field {
            caption: qsTr("Completed rank")
            value: root.trackingVm ? root.trackingVm.completedRank : ""
            valueColor: root._color("completedRankColor")
            valueObjectName: "summaryCompletedRank"
        }
        // The average rank on the tier bar the progress grid uses: Iron, Bronze and
        // Silver full and Gold half full at 3.5, with a tick where the completed rank's
        // chunk ends. Where it sits ("Bronze · 60% to Silver") is in the tooltip.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Average rank")
                color: root.palette.placeholderText
                Layout.preferredWidth: 120
            }
            Item {
                id: averageRankArea
                objectName: "summaryAverageRank"
                readonly property string text: root.trackingVm ? String(root.trackingVm.averageRank || "") : ""
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.minimumWidth: 160
                implicitHeight: averageRankBar.implicitHeight + 8
                Accessible.role: Accessible.ProgressBar
                Accessible.name: qsTr("Average rank: %1").arg(text)
                Accessible.description: qsTr("Average rank: %1").arg(text)

                TierBar {
                    id: averageRankBar
                    objectName: "summaryAverageRankBar"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    tiers: root.trackingVm && root.trackingVm.progressGrid ? root.trackingVm.progressGrid.tiers : []
                    fills: root.trackingVm && root.trackingVm.averageRankFills ? root.trackingVm.averageRankFills : []
                    tick: root.trackingVm && root.trackingVm.completedRankPosition !== undefined
                          ? Math.round(root.trackingVm.completedRankPosition) : -1
                    tickDescription: root.trackingVm
                                     ? qsTr("Completed rank: %1").arg(String(root.trackingVm.completedRank || "")) : ""
                }
                HoverHandler { id: averageRankHover }
                ToolTip.visible: averageRankHover.hovered && !averageRankBar.tickHovered
                ToolTip.delay: Theme.shortToolTipDelay
                ToolTip.text: qsTr("Average rank: %1").arg(averageRankArea.text)
            }
        }
        Field {
            caption: qsTr("Total playtime")
            value: root.trackingVm ? root.trackingVm.totalPlaytime : ""
            valueObjectName: "summaryTotalPlaytime"
        }
        Field {
            caption: qsTr("Next tier")
            value: root.trackingVm ? root.trackingVm.nextTier : ""
            valueColor: root._color("nextTierColor")
            valueObjectName: "summaryNextTier"
            suffix: root._blockerSuffix()
            suffixObjectName: "summaryNextTierBlockers"
        }
    }
}
