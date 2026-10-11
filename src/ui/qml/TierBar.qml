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

    // --- edge states -------------------------------------------------------------
    // One flag per tier: that chunk's thresholds are unusable, so it is hatched.
    property var missing: []
    // "" | "unplayed" | "unmatched" | "aboveTop"; statusText labels the first two.
    property string edgeState: ""
    property string statusText: ""
    // Tooltip of the above-the-top-tier mark.
    property string aboveTopText: ""
    readonly property color unmatchedColor: "#FFB74D"

    function missingAt(index) {
        return root.missing && root.missing.length > index && root.missing[index] === true
    }

    // Unmatched or ambiguous mapping: the bar is only outlined, and says what to fix.
    Rectangle {
        objectName: root.objectName.length > 0 ? root.objectName + "_outline" : ""
        visible: root.edgeState === "unmatched"
        z: 2
        anchors.fill: parent
        radius: 2
        color: root.palette.base
        border.color: root.unmatchedColor
        border.width: 1
    }
    // Unplayed or unmatched: a short status inside the (empty) bar.
    Label {
        objectName: root.objectName.length > 0 ? root.objectName + "_status" : ""
        visible: (root.edgeState === "unplayed" || root.edgeState === "unmatched") && root.statusText.length > 0
        z: 3
        anchors.centerIn: parent
        width: Math.min(implicitWidth, Math.max(0, root.width - 8))
        elide: Text.ElideRight
        // Sized to the bar so the label stays inside it.
        font.pixelSize: Math.max(9, Math.round(root.height * 0.7))
        color: root.edgeState === "unmatched" ? root.unmatchedColor : root.palette.placeholderText
        font.italic: root.edgeState === "unplayed"
        text: root.statusText
    }
    // Above the top tier: a small mark in the top tier's colour past the bar's end.
    Item {
        objectName: root.objectName.length > 0 ? root.objectName + "_aboveTop" : ""
        visible: root.edgeState === "aboveTop" && root.chunkCount > 0
        z: 2
        x: root.width - 3
        width: 9
        height: root.height
        Accessible.role: Accessible.Indicator
        Accessible.name: root.aboveTopText

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 7
            rotation: 45
            color: root.chunkCount > 0 && root.tiers[root.chunkCount - 1]
                   ? String(root.tiers[root.chunkCount - 1].barColor || "") : root.palette.highlight
            border.color: root.palette.base
            border.width: 1
        }
        HoverHandler { id: aboveTopHover }
        ToolTip.visible: aboveTopHover.hovered && root.aboveTopText.length > 0
        ToolTip.delay: 300
        ToolTip.text: root.aboveTopText
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
            // Missing thresholds: diagonal hatching instead of any fill.
            Item {
                objectName: root.objectName.length > 0 ? root.objectName + "_hatch_" + chunk.index : ""
                visible: root.missingAt(chunk.index)
                anchors.fill: parent
                clip: true
                Repeater {
                    model: parent.visible ? Math.ceil((chunk.width + chunk.height) / 6) + 1 : 0
                    delegate: Rectangle {
                        required property int index
                        x: index * 6 - chunk.height
                        y: -chunk.height / 2
                        width: 2
                        height: chunk.height * 2
                        rotation: 45
                        color: Qt.alpha(root.palette.text, 0.35)
                    }
                }
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
