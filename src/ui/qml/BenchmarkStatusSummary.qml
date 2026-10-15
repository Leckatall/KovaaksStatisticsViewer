import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var trackingVm

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    component Field: RowLayout {
        id: fieldRow
        property string caption
        property string value
        property string valueObjectName
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
        }
    }

    function _color(name) {
        if (!root.trackingVm) return ""
        var value = root.trackingVm[name]
        return value === undefined || value === null ? "" : String(value)
    }

    GridLayout {
        anchors.fill: parent
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
                ToolTip.delay: 300
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
        }
    }
}
