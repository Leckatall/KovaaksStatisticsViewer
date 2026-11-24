pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Rectangle {
    id: groupCell
    required property var modelData
    required property int index
    property var grid: null
    readonly property var column: groupCell.grid.columns[String(modelData.column)]
    readonly property bool spansBoth: groupCell.grid.spansSubcategory(modelData)
    readonly property string tint: String(modelData.color || "")
    readonly property bool hasRank: String(modelData.rankText || "").length > 0
    readonly property var layout: groupCell.grid.groupLayouts[groupCell.index] || ({lines: [], chipBeside: false, contentHeight: 0})
    readonly property int start: Number(modelData.start)
    readonly property real firstRowHeight: groupCell.grid.rowTops[start + 1] - groupCell.grid.rowTops[start]

    objectName: "progressGroup_" + (String(modelData.id).length > 0 ? modelData.id : "uncategorized")
    visible: column.width > 0
    z: 3
    x: column.x + groupCell.grid.pinOffset
    y: groupCell.grid.rowTops[start]
    width: spansBoth ? column.width + groupCell.grid.columns.subcategory.width : column.width
    height: groupCell.grid.rowTops[start + Number(modelData.span)] - groupCell.grid.rowTops[start]
    color: groupCell.grid.palette.base
    Accessible.role: Accessible.Cell
    Accessible.name: String(modelData.name)

    HoverHandler { blocking: groupCell.grid.pinning }
    Rectangle {
        anchors.fill: parent
        visible: groupCell.tint.length > 0
        color: groupCell.tint.length > 0 ? Qt.alpha(groupCell.tint, Theme.groupTintAlpha) : "transparent"
    }
    GridCellBorders {}
    // Centred beside the group's first scenario when it fits there, so a short name stays
    // level with it. The grid decides whether the chip fits beside the name.
    Grid {
        objectName: groupCell.objectName + "_content"
        x: groupCell.grid.cellPadding
        y: Math.max(groupCell.grid.groupVerticalPadding, (groupCell.firstRowHeight - groupCell.layout.contentHeight) / 2)
        columns: groupCell.layout.chipBeside === true ? 2 : 1
        columnSpacing: groupCell.grid.rankChipSpacing
        rowSpacing: groupCell.grid.groupLineGap
        verticalItemAlignment: Grid.AlignVCenter

        // The grid wraps the name and measures the rows from those exact lines.
        Label {
            objectName: groupCell.objectName + "_name"
            font.bold: true
            // A name can contain anything; rich-text detection would drop the line breaks.
            textFormat: Text.PlainText
            lineHeightMode: Text.FixedHeight
            lineHeight: groupCell.grid.groupLineHeight
            text: groupCell.layout.lines.join("\n")
        }
        // From the data, not rankChip.visible: that reads false while any ancestor is hidden, and
        // binding the Row to it would then keep the chip hidden for good.
        Row {
            visible: groupCell.hasRank
            spacing: groupCell.grid.rankChipSpacing

            // The group's rank in its tier colour; neutral when unranked.
            Rectangle {
                id: rankChip
                readonly property string tierColor: String(groupCell.modelData.rankBarColor || "")
                objectName: groupCell.objectName + "_rank"
                width: rankLabel.implicitWidth + 2 * groupCell.grid.rankChipPadding
                height: groupCell.grid.groupChipHeight
                radius: height / 2
                color: rankChip.tierColor.length > 0 ? Qt.alpha(rankChip.tierColor, 0.28) : groupCell.grid.palette.base
                border.color: rankChip.tierColor.length > 0 ? rankChip.tierColor : groupCell.grid.palette.mid
                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("%1 rank: %2").arg(String(groupCell.modelData.name)).arg(rankLabel.text)

                Label {
                    id: rankLabel
                    anchors.centerIn: parent
                    font.pixelSize: 11
                    text: String(groupCell.modelData.rankText || "")
                    color: String(groupCell.modelData.rankColor || "").length > 0
                           ? groupCell.modelData.rankColor : groupCell.grid.palette.placeholderText
                }
            }
            // Blocker marker, in the next tier's colour: the group keeps the benchmark
            // from that tier until any one of its scenarios reaches it.
            Item {
                objectName: groupCell.modelData.blocker === true ? groupCell.objectName + "_blocker" : ""
                visible: groupCell.modelData.blocker === true
                anchors.verticalCenter: rankChip.verticalCenter
                width: groupCell.grid.groupMarkerSize
                height: groupCell.grid.groupMarkerSize
                Accessible.role: Accessible.Indicator
                Accessible.name: String(groupCell.modelData.blockerText || "")

                Rectangle {
                    anchors.centerIn: parent
                    width: 9
                    height: 9
                    rotation: 45
                    color: String(groupCell.modelData.blockerColor || "") || groupCell.grid.palette.highlight
                }
                HoverHandler { id: blockerHover }
                ToolTip.visible: blockerHover.hovered
                ToolTip.delay: Theme.shortToolTipDelay
                ToolTip.text: String(groupCell.modelData.blockerText || "")
            }
        }
    }
}
