import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Rectangle {
    id: groupCell
    required property var modelData
    property Item grid: null
    readonly property var column: grid.columns[String(modelData.column)]
    readonly property bool spansBoth: grid.spansSubcategory(modelData)
    readonly property string tint: String(modelData.color || "")

    objectName: "progressGroup_" + (String(modelData.id).length > 0 ? modelData.id : "uncategorized")
    visible: column.width > 0
    z: 3
    x: column.x + grid.pinOffset
    y: grid.headerHeight + Number(modelData.start) * grid.rowHeight
    width: spansBoth ? column.width + grid.columns.subcategory.width : column.width
    height: Number(modelData.span) * grid.rowHeight
    color: grid.palette.base
    Accessible.role: Accessible.Cell
    Accessible.name: String(modelData.name)

    HoverHandler { blocking: grid.pinning }
    Rectangle {
        anchors.fill: parent
        visible: groupCell.tint.length > 0
        color: groupCell.tint.length > 0 ? Qt.alpha(groupCell.tint, Theme.groupTintAlpha) : "transparent"
    }
    GridCellBorders {}
    Row {
        x: grid.cellPadding
        width: Math.max(0, groupCell.width - 2 * grid.cellPadding)
        height: grid.rowHeight
        spacing: grid.rankChipSpacing

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
            width: rankLabel.implicitWidth + 2 * grid.rankChipPadding
            height: rankLabel.implicitHeight + 4
            radius: height / 2
            color: tierColor.length > 0 ? Qt.alpha(tierColor, 0.28) : grid.palette.base
            border.color: tierColor.length > 0 ? tierColor : grid.palette.mid
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("%1 rank: %2").arg(String(groupCell.modelData.name)).arg(rankLabel.text)

            Label {
                id: rankLabel
                anchors.centerIn: parent
                font.pixelSize: 11
                text: String(groupCell.modelData.rankText || "")
                color: String(groupCell.modelData.rankColor || "").length > 0
                       ? groupCell.modelData.rankColor : grid.palette.placeholderText
            }
        }
    }
}
