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

    HoverHandler { id: barHover }
    ToolTip.visible: barHover.hovered && root.description.length > 0
    ToolTip.delay: 300
    ToolTip.text: root.description
}
