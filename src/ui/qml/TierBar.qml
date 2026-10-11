import QtQuick
import QtQuick.Controls

// A segmented tier bar: one equal-width chunk per tier (Iron ... the top tier), each
// filled by how far the value has come from the previous tier's threshold toward
// its own, in that tier's colour, over a dim neutral track. Chunk boundaries come
// from chunkLeft()/chunkRight() alone, so a header laid out with the same functions
// lines up with every bar. The whole bar is the tooltip's hover target.
Item {
    id: root

    // [{name, barColor}] in ladder order.
    property var tiers: []
    // One fill fraction in [0, 1] per tier.
    property var fills: []
    // Tooltip and accessible text: where the value sits on the ladder.
    property string description: ""

    readonly property int chunkCount: tiers ? tiers.length : 0
    readonly property real chunkGap: 2
    readonly property color trackColor: Qt.alpha(root.palette.text, 0.12)

    implicitWidth: 240
    implicitHeight: 16

    // Chunk `index`'s left/right edge in this item's coordinates: equal shares of the
    // width, with half a gap taken from each side of an inner boundary.
    function chunkLeft(index) {
        if (chunkCount <= 0) return 0
        return Math.round(index * width / chunkCount) + (index > 0 ? chunkGap / 2 : 0)
    }
    function chunkRight(index) {
        if (chunkCount <= 0) return 0
        return Math.round((index + 1) * width / chunkCount) - (index < chunkCount - 1 ? chunkGap / 2 : 0)
    }
    function fillAt(index) {
        const value = Number(root.fills && root.fills.length > index ? root.fills[index] : 0)
        return isNaN(value) ? 0 : Math.max(0, Math.min(1, value))
    }

    Accessible.role: Accessible.ProgressBar
    Accessible.name: root.description

    Repeater {
        model: root.chunkCount
        delegate: Item {
            id: chunk
            required property int index
            readonly property real fill: root.fillAt(index)
            readonly property var tier: root.tiers[index]

            objectName: root.objectName.length > 0 ? root.objectName + "_chunk_" + index : ""
            x: root.chunkLeft(index)
            width: Math.max(0, root.chunkRight(index) - x)
            height: root.height

            Rectangle {
                anchors.fill: parent
                radius: 2
                color: root.trackColor
            }
            Rectangle {
                objectName: root.objectName.length > 0 ? root.objectName + "_fill_" + chunk.index : ""
                width: Math.round(parent.width * chunk.fill)
                height: parent.height
                radius: 2
                visible: width > 0
                color: chunk.tier && String(chunk.tier.barColor || "").length > 0
                       ? chunk.tier.barColor : root.palette.highlight
            }
        }
    }

    // --- recent-average marker ------------------------------------------------
    // A position on the chunk scale (chunk units, 0 = bar start, chunkCount = bar
    // end), or negative for no marker, with its own tooltip.
    property real marker: -1
    property string markerDescription: ""

    // Scale position to x, across the gaps between chunks.
    function scaleX(position) {
        if (chunkCount <= 0) return 0
        const clamped = Math.max(0, Math.min(chunkCount, position))
        const index = Math.min(chunkCount - 1, Math.floor(clamped))
        return chunkLeft(index) + (clamped - index) * (chunkRight(index) - chunkLeft(index))
    }

    Item {
        id: markerItem
        objectName: root.objectName.length > 0 ? root.objectName + "_marker" : ""
        visible: root.marker >= 0 && root.chunkCount > 0
        // A wider hover target than the tick it centres on.
        width: 9
        height: root.height + 6
        x: Math.round(root.scaleX(root.marker) - width / 2)
        y: -3
        z: 1
        Accessible.role: Accessible.Indicator
        Accessible.name: root.markerDescription

        // A light tick outlined in the base colour reads both over a fill and over
        // the empty track, and overhangs the bar so its ends always show.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 4
            height: parent.height
            radius: 1
            color: root.palette.text
            border.color: root.palette.base
            border.width: 1
        }
        HoverHandler { id: markerHover }
        ToolTip.visible: markerHover.hovered && root.markerDescription.length > 0
        ToolTip.delay: 300
        ToolTip.text: root.markerDescription
    }

    HoverHandler { id: barHover }
    ToolTip.visible: barHover.hovered && !markerHover.hovered && root.description.length > 0
    ToolTip.delay: 300
    ToolTip.text: root.description
}
