pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Item {
    id: rowItem
    required property var modelData
    required property int index
    property var grid: null

    component ValueCell: Label {
        required property real columnX
        required property real columnWidth
        x: columnX + rowItem.grid.cellPadding
        width: Math.max(0, columnWidth - 2 * rowItem.grid.cellPadding)
        height: rowItem.grid.rowHeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        color: text === "—" ? rowItem.grid.palette.placeholderText : rowItem.grid.palette.text
    }

    objectName: "progressRow_" + modelData.entryId
    x: 0
    y: rowItem.grid.headerHeight + index * rowItem.grid.rowHeight
    width: rowItem.grid.columns.total
    height: rowItem.grid.rowHeight
    Accessible.role: Accessible.Row
    Accessible.name: String(modelData.name)

    Rectangle {
        anchors.fill: parent
        color: rowItem.grid.palette.base
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: rowItem.grid.palette.mid
    }
    // The pinned columns' own background, so scrolled cells pass under it.
    Rectangle {
        z: 1
        visible: rowItem.grid.pinning
        x: rowItem.grid.pinOffset
        width: rowItem.grid.pinnedWidth
        height: rowItem.height - 1
        color: rowItem.grid.palette.base
        HoverHandler { blocking: true }
    }
    Repeater {
        model: ["category", "subcategory", "scenario", "pbDate", "plays", "progress", "next"]
        delegate: Rectangle {
            id: columnLine
            required property string modelData
            readonly property bool pinned: columnLine.modelData === "category" || columnLine.modelData === "subcategory"
                                           || columnLine.modelData === "scenario"
            visible: rowItem.grid.columns[columnLine.modelData].width > 0
            z: columnLine.pinned ? 2 : 0
            x: rowItem.grid.columns[columnLine.modelData].x + rowItem.grid.columns[columnLine.modelData].width - 1
               + (columnLine.pinned ? rowItem.grid.pinOffset : 0)
            width: 1
            height: rowItem.height
            color: rowItem.grid.palette.mid
        }
    }

    // Blocker marker, in the next tier's colour.
    Item {
        objectName: rowItem.modelData.blocker === true ? "progressRow_" + rowItem.modelData.entryId + "_blocker" : ""
        visible: rowItem.modelData.blocker === true
        z: 2
        x: rowItem.grid.columns.scenario.x + rowItem.grid.pinOffset + rowItem.grid.cellPadding - 4
        width: rowItem.grid.markerSpace
        height: rowItem.height
        Accessible.role: Accessible.Indicator
        Accessible.name: String(rowItem.modelData.blockerText || "")

        Rectangle {
            anchors.centerIn: parent
            width: 9
            height: 9
            rotation: 45
            color: String(rowItem.modelData.blockerColor || "") || rowItem.grid.palette.highlight
        }
        HoverHandler { id: blockerHover }
        ToolTip.visible: blockerHover.hovered
        ToolTip.delay: Theme.shortToolTipDelay
        ToolTip.text: String(rowItem.modelData.blockerText || "")
    }

    Label {
        id: nameLabel
        objectName: "progressRow_" + rowItem.modelData.entryId + "_name"
        z: 2
        x: rowItem.grid.columns.scenario.x + rowItem.grid.pinOffset + rowItem.grid.cellPadding + rowItem.grid.markerSpace
        width: Math.max(0, rowItem.grid.columns.scenario.width - 2 * rowItem.grid.cellPadding - rowItem.grid.markerSpace)
        height: rowItem.height
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: String(rowItem.modelData.name)

        HoverHandler { id: nameHover }
        ToolTip.visible: nameHover.hovered && nameLabel.truncated
        ToolTip.delay: Theme.toolTipDelay
        ToolTip.text: nameLabel.text
    }

    TierBar {
        objectName: "progressRow_" + rowItem.modelData.entryId + "_bar"
        x: rowItem.grid.barX
        y: Math.round((rowItem.height - height) / 2)
        width: rowItem.grid.barWidth
        height: rowItem.grid.barHeight
        marker: rowItem.modelData.recentPosition !== undefined
                ? Number(rowItem.modelData.recentPosition) : -1
        markerDescription: String(rowItem.modelData.recentDescription || "")
        tiers: rowItem.grid.tiers
        fills: rowItem.modelData.fills || []
        labels: rowItem.modelData.thresholdTexts || []
        valueText: rowItem.modelData.hasPersonalBest
                   ? String(rowItem.modelData.personalBestText || "") : ""
        valueColor: String(rowItem.modelData.attainedTierColor || "") || palette.placeholderText
        description: String(rowItem.modelData.barDescription || "")
        missing: rowItem.modelData.missingChunks || []
        edgeState: String(rowItem.modelData.edgeState || "")
        statusText: String(rowItem.modelData.statusText || "")
        aboveTopText: String(rowItem.modelData.aboveTopText || "")
    }

    ValueCell {
        objectName: "progressRow_" + rowItem.modelData.entryId + "_personalBestDate"
        columnX: rowItem.grid.columns.pbDate.x
        columnWidth: rowItem.grid.columns.pbDate.width
        text: String(rowItem.modelData.personalBestDateText)
        Accessible.name: qsTr("PB date %1").arg(text)
    }
    ValueCell {
        objectName: "progressRow_" + rowItem.modelData.entryId + "_playCount"
        columnX: rowItem.grid.columns.plays.x
        columnWidth: rowItem.grid.columns.plays.width
        text: String(rowItem.modelData.playCountText)
        Accessible.name: qsTr("Plays %1").arg(text)
    }
    Row {
        objectName: "progressRow_" + rowItem.modelData.entryId + "_next"
        x: rowItem.grid.columns.next.x + rowItem.grid.cellPadding
        width: Math.max(0, rowItem.grid.columns.next.width - 2 * rowItem.grid.cellPadding)
        height: rowItem.height
        layoutDirection: Qt.RightToLeft
        spacing: 4
        clip: true
        // Read by tests and assistive tech as one value.
        readonly property string text: String(rowItem.modelData.nextText)
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("Next tier %1").arg(text)

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: String(rowItem.modelData.nextThresholdText)
            color: String(rowItem.modelData.nextTierText || "").length > 0
                   ? rowItem.grid.palette.text : rowItem.grid.palette.placeholderText
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            visible: text.length > 0
            text: String(rowItem.modelData.nextTierText || "")
            color: String(rowItem.modelData.nextTierColor || "").length > 0
                   ? rowItem.modelData.nextTierColor : rowItem.grid.palette.text
        }
    }
}
