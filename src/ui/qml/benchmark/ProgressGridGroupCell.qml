pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Rectangle {
    id: groupCell
    required property var modelData
    property var grid: null
    readonly property var column: groupCell.grid.columns[String(modelData.column)]
    readonly property bool spansBoth: groupCell.grid.spansSubcategory(modelData)
    readonly property string tint: String(modelData.color || "")

    objectName: "progressGroup_" + (String(modelData.id).length > 0 ? modelData.id : "uncategorized")
    visible: column.width > 0
    z: 3
    x: column.x + groupCell.grid.pinOffset
    y: groupCell.grid.headerHeight + Number(modelData.start) * groupCell.grid.rowHeight
    width: spansBoth ? column.width + groupCell.grid.columns.subcategory.width : column.width
    height: Number(modelData.span) * groupCell.grid.rowHeight
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
    Row {
        x: groupCell.grid.cellPadding
        width: Math.max(0, groupCell.width - 2 * groupCell.grid.cellPadding)
        height: groupCell.grid.rowHeight
        spacing: groupCell.grid.rankChipSpacing

        Label {
            objectName: groupCell.objectName + "_name"
            width: Math.max(0, Math.min(implicitWidth, parent.width - (rankChip.visible
                                        ? rankChip.width + parent.spacing : 0)))
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.bold: true
            text: String(groupCell.modelData.name)
        }
        // The group's rank in its tier colour; neutral when unranked.
        Rectangle {
            id: rankChip
            readonly property string tierColor: String(groupCell.modelData.rankBarColor || "")
            objectName: groupCell.objectName + "_rank"
            visible: String(groupCell.modelData.rankText || "").length > 0
            anchors.verticalCenter: parent.verticalCenter
            width: rankLabel.implicitWidth + 2 * groupCell.grid.rankChipPadding
            height: rankLabel.implicitHeight + 4
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
    }
}
