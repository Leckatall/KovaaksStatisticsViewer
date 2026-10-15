import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// A segmented tier bar: one equal-width chunk per tier (Iron ... the top tier), each
// the progress toward that tier, filled by how far the value has come from the previous
// tier's threshold (0 for the first tier) toward its own, in that tier's colour, over a
// dim neutral track. A chunk is full exactly when its tier is reached, so the filled
// length in chunks is the normalized rank (6.37 = six full chunks). Chunk boundaries come
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
    // The fill's drawn width in chunk `index`.
    function fillWidthAt(index) {
        return Math.round(Math.max(0, chunkRight(index) - chunkLeft(index)) * fillAt(index))
    }

    // --- edge states -------------------------------------------------------------
    // One flag per tier: that chunk's thresholds are unusable, so it is hatched.
    property var missing: []
    // "" | "unplayed" | "unmatched" | "aboveTop"; statusText labels the first two.
    property string edgeState: ""
    property string statusText: ""
    // Tooltip of the above-the-top-tier mark.
    property string aboveTopText: ""
    readonly property color unmatchedColor: Theme.attention

    function missingAt(index) {
        return root.missing && root.missing.length > index && root.missing[index] === true
    }

    // One short text per tier, right-aligned at the end of its chunk (the score that reaches
    // the tier, which completes the chunk); "" or absent for none. Hidden under a hatched
    // chunk and behind a status label. Where the value label or the recent-average marker
    // would cover it, it moves to its chunk's middle, then to its start; see labelPlacementAt().
    property var labels: []
    function labelAt(index) {
        return root.labels && root.labels.length > index ? String(root.labels[index] || "") : ""
    }

    // --- value label ---------------------------------------------------------------
    // The value the fill stands for, just past the fill's end in valueColor; inside the
    // fill's end instead when it would run off the bar. It never moves for a tier label:
    // the tier label moves out of its way instead.
    property string valueText: ""
    property color valueColor: root.palette.text
    readonly property real valueGap: 4
    // The right edge of the furthest fill, or 0 for none.
    readonly property real fillEnd: {
        for (let i = root.chunkCount - 1; i >= 0; --i) {
            const fillWidth = root.fillWidthAt(i)
            if (fillWidth > 0) return root.chunkLeft(i) + fillWidth
        }
        return 0
    }
    readonly property bool valueShown: valueText.length > 0 && chunkCount > 0
                                       && edgeState !== "unplayed" && edgeState !== "unmatched"
    readonly property bool valueInside: fillEnd + valueGap + valueLabel.implicitWidth
                                        > width - (edgeState === "aboveTop" ? 4 : 0)
    readonly property real valueLeft: valueInside
                                      ? Math.max(0, fillEnd - valueGap - (edgeState === "aboveTop" ? 4 : 0)
                                                    - valueLabel.implicitWidth)
                                      : fillEnd + valueGap
    readonly property real valueRight: valueLeft + valueLabel.implicitWidth
    // The left edge of a chunk label of `labelWidth` in chunk `index` at `placement`
    // ("end", "middle" or "start"); its width is labelSpanWidth().
    function labelSpanWidth(index, labelWidth) {
        return Math.min(labelWidth, Math.max(0, chunkRight(index) - chunkLeft(index) - 2 * labelInset))
    }
    function labelSpanLeft(index, labelWidth, placement) {
        const shown = labelSpanWidth(index, labelWidth)
        if (placement === "start") return chunkLeft(index) + labelInset
        if (placement === "middle") return (chunkLeft(index) + chunkRight(index) - shown) / 2
        return chunkRight(index) - labelInset - shown
    }
    // Whether that label would touch the value label.
    function coversValue(index, labelWidth, placement) {
        if (!valueShown) return false
        const left = labelSpanLeft(index, labelWidth, placement)
        return left + labelSpanWidth(index, labelWidth) > valueLeft - valueGap && left < valueRight + valueGap
    }
    // Whether that label would sit under the recent-average marker's tick, or within
    // markerGap of it. A label that would be a target there is measured with its outline
    // (targetPad past the text each side), so the box stays clear of the line too.
    function coversMarker(index, labelWidth, placement) {
        if (marker < 0 || chunkCount <= 0) return false
        const x = scaleX(marker)
        const pad = labelInPartialFillAt(index, labelWidth, placement) ? targetPad : 0
        const left = labelSpanLeft(index, labelWidth, placement) - pad
        const right = left + labelSpanWidth(index, labelWidth) + 2 * pad
        return right > x - markerHalfWidth - markerGap && left < x + markerHalfWidth + markerGap
    }
    // Where chunk `index`'s label of `labelWidth` goes: the first of the end of its chunk,
    // its middle and its start that is clear of both the value label and the recent-average
    // marker. When none is clear of both, the first clear of the value label, even under
    // the marker; "" when the value covers all three. The value label itself never moves.
    function labelPlacementAt(index, labelWidth) {
        const clearOfValue = ["end", "middle", "start"].filter(p => !coversValue(index, labelWidth, p))
        const clear = clearOfValue.filter(p => !coversMarker(index, labelWidth, p))
        return clear.length > 0 ? clear[0] : clearOfValue.length > 0 ? clearOfValue[0] : ""
    }
    // Whether chunk `index`'s label of `labelWidth` at `placement` sits (even partly) over
    // the filled part of a chunk the fill covers only partly. Such a label is a score not
    // yet reached, so it is drawn as a target (outlined) rather than as plain text, which
    // over the fill would read as a score already reached. A label on a full chunk or over
    // the empty track keeps the plain look.
    function labelInPartialFillAt(index, labelWidth, placement) {
        if (placement.length === 0) return false
        const fillWidth = fillWidthAt(index)
        if (fillWidth <= 0 || fillWidth >= chunkRight(index) - chunkLeft(index)) return false
        return labelSpanLeft(index, labelWidth, placement) < chunkLeft(index) + fillWidth
    }
    function labelAlignment(placement) {
        return placement === "start" ? Text.AlignLeft
             : placement === "middle" ? Text.AlignHCenter : Text.AlignRight
    }
    readonly property real labelInset: 3
    // How far a target label's outline reaches past its text on each side.
    readonly property real targetPad: 2

    Label {
        id: valueLabel
        objectName: root.objectName.length > 0 ? root.objectName + "_value" : ""
        visible: root.valueShown
        z: 0.5
        x: Math.round(root.valueLeft)
        height: root.height
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Math.max(8, Math.round(root.height * 0.62))
        font.bold: true
        // Inside the fill it sits on a tier colour, where the rank colour may not read.
        color: {
            if (!root.valueInside) return root.valueColor
            for (let i = root.chunkCount - 1; i >= 0; --i) {
                if (root.fillAt(i) > 0) {
                    const tier = root.tiers[i]
                    const c = Qt.color(tier && String(tier.barColor || "").length > 0 ? tier.barColor : root.palette.highlight)
                    return Theme.isLightFill(c)
                           ? Qt.rgba(0.1, 0.1, 0.1, 0.9) : Qt.rgba(1, 1, 1, 0.95)
                }
            }
            return root.valueColor
        }
        text: root.valueText
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

        // A right-pointing chevron ("beyond the bar"); the row's blocker marker stays a diamond.
        Canvas {
            id: aboveTopChevron
            objectName: root.objectName.length > 0 ? root.objectName + "_aboveTopChevron" : ""
            anchors.centerIn: parent
            width: 7
            height: 10
            property color strokeColor: root.chunkCount > 0 && root.tiers[root.chunkCount - 1]
                                        ? String(root.tiers[root.chunkCount - 1].barColor || "")
                                        : root.palette.highlight
            property color haloColor: root.palette.base
            onStrokeColorChanged: requestPaint()
            onHaloColorChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(1.5, 1.5)
                ctx.lineTo(width - 1.5, height / 2)
                ctx.lineTo(1.5, height - 1.5)
                ctx.strokeStyle = haloColor
                ctx.lineWidth = 4
                ctx.stroke()
                ctx.strokeStyle = strokeColor
                ctx.lineWidth = 2
                ctx.stroke()
            }
        }
        HoverHandler { id: aboveTopHover }
        ToolTip.visible: aboveTopHover.hovered && root.aboveTopText.length > 0
        ToolTip.delay: Theme.shortToolTipDelay
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
            readonly property color fillColor: tier && String(tier.barColor || "").length > 0
                                               ? tier.barColor : root.palette.highlight
            // Dark text on a light tier colour, light text on a dark one.
            readonly property color onFillColor: Theme.isLightFill(fillColor)
                                                 ? Qt.rgba(0.1, 0.1, 0.1, 0.85) : Qt.rgba(1, 1, 1, 0.9)
            readonly property string labelText: root.labelAt(index)
            // "end" | "middle" | "start": where the label sits, clear of the value; "" for nowhere.
            readonly property string labelPlacement: root.labelPlacementAt(index, thresholdLabel.implicitWidth)
            readonly property bool labelShown: labelText.length > 0 && !root.missingAt(index)
                                               && root.edgeState !== "unplayed" && root.edgeState !== "unmatched"
                                               && labelPlacement.length > 0
            // The label is a not-yet-reached score over the fill: it gets the target outline.
            readonly property bool labelIsTarget: labelShown
                                                  && root.labelInPartialFillAt(index, thresholdLabel.implicitWidth, labelPlacement)
            // The label's drawn span in this chunk's coordinates (the outline goes around it).
            readonly property real labelSpanX: root.labelSpanLeft(index, thresholdLabel.implicitWidth, labelPlacement) - x
            readonly property real labelSpanW: root.labelSpanWidth(index, thresholdLabel.implicitWidth)
            // Scales with the bar, so it keeps its margin in a taller or shorter one.
            readonly property real labelPixelSize: Math.max(8, Math.round(root.height * 0.62))

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
                id: fillRect
                objectName: root.objectName.length > 0 ? root.objectName + "_fill_" + chunk.index : ""
                width: root.fillWidthAt(chunk.index)
                height: parent.height
                radius: 2
                visible: width > 0
                color: chunk.fillColor
            }
            // The score that reaches this tier, at the end of its chunk (or wherever
            // labelPlacement moved it). One label dim over
            // the empty track and one, clipped to the fill, in whichever of dark and light reads
            // on the tier colour, so the number is legible across the fill's edge.
            Label {
                id: thresholdLabel
                objectName: root.objectName.length > 0 ? root.objectName + "_label_" + chunk.index : ""
                visible: chunk.labelShown
                anchors.fill: parent
                anchors.leftMargin: root.labelInset
                anchors.rightMargin: root.labelInset
                horizontalAlignment: root.labelAlignment(chunk.labelPlacement)
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                font.pixelSize: chunk.labelPixelSize
                color: Qt.alpha(root.palette.text, 0.62)
                text: chunk.labelText
            }
            // A target label's outline: a thin box around the score, so a score sitting in
            // the fill reads as the mark still to reach. Drawn twice like the label, dim over
            // the empty track and in the label's fill colour inside the fill.
            Rectangle {
                id: targetOutline
                objectName: root.objectName.length > 0 ? root.objectName + "_target_" + chunk.index : ""
                visible: chunk.labelIsTarget
                x: Math.round(chunk.labelSpanX) - root.targetPad
                y: 1
                width: Math.round(chunk.labelSpanW) + 2 * root.targetPad
                height: Math.max(0, parent.height - 2)
                radius: 2
                color: "transparent"
                border.width: 1
                border.color: Qt.alpha(root.palette.text, 0.62)
            }
            Item {
                visible: chunk.labelShown && fillRect.visible
                width: fillRect.width
                height: parent.height
                clip: true
                Rectangle {
                    visible: chunk.labelIsTarget
                    x: targetOutline.x
                    y: targetOutline.y
                    width: targetOutline.width
                    height: targetOutline.height
                    radius: targetOutline.radius
                    color: "transparent"
                    border.width: 1
                    border.color: chunk.onFillColor
                }
                Label {
                    width: chunk.width
                    height: parent.height
                    leftPadding: root.labelInset
                    rightPadding: root.labelInset
                    horizontalAlignment: root.labelAlignment(chunk.labelPlacement)
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: chunk.labelPixelSize
                    color: chunk.onFillColor
                    text: chunk.labelText
                }
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
    // Half the tick's drawn width, and the gap a threshold label keeps from it.
    readonly property real markerHalfWidth: 2
    readonly property real markerGap: 2
    // The marker passes through the value label, which never moves: the tick is then cut
    // where the text is and drawn only as its ends above and below the bar, so the value
    // stays legible and the ends still point at the marker's place.
    readonly property bool markerCrossesValue: {
        if (!valueShown || marker < 0 || chunkCount <= 0) return false
        const x = scaleX(marker)
        return x + markerHalfWidth + 1 > valueLeft && x - markerHalfWidth - 1 < valueRight
    }

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
        // Cut behind the value label (markerCrossesValue): only the overhanging ends, each
        // reaching into the bar just short of the text's digits.
        readonly property real endLength: Math.max(3, 3 + Math.round((root.height - valueLabel.font.pixelSize) / 2) - 1)
        Rectangle {
            objectName: root.objectName.length > 0 ? root.objectName + "_markerTick" : ""
            anchors.horizontalCenter: parent.horizontalCenter
            width: 2 * root.markerHalfWidth
            height: root.markerCrossesValue ? markerItem.endLength : parent.height
            radius: 1
            color: root.palette.text
            border.color: root.palette.base
            border.width: 1
        }
        Rectangle {
            objectName: root.objectName.length > 0 ? root.objectName + "_markerTickBottom" : ""
            visible: root.markerCrossesValue
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: 2 * root.markerHalfWidth
            height: markerItem.endLength
            radius: 1
            color: root.palette.text
            border.color: root.palette.base
            border.width: 1
        }
        HoverHandler { id: markerHover }
        ToolTip.visible: markerHover.hovered && root.markerDescription.length > 0
        ToolTip.delay: Theme.shortToolTipDelay
        ToolTip.text: root.markerDescription
    }

    HoverHandler { id: barHover }
    ToolTip.visible: barHover.hovered && !markerHover.hovered && root.description.length > 0
    ToolTip.delay: Theme.shortToolTipDelay
    ToolTip.text: root.description

    // --- tier tick -----------------------------------------------------------------
    // A tick at position `tick` on the chunk scale: the boundary where chunk `tick` - 1
    // ends and chunk `tick` starts (0 = the bar's start, chunkCount = the bar's end), or
    // none when negative, with its own tooltip. Read tickHovered to
    // keep a surrounding tooltip out of its way.
    property int tick: -1
    property string tickDescription: ""
    readonly property bool tickHovered: tickHover.hovered

    Item {
        id: tickItem
        objectName: root.objectName.length > 0 ? root.objectName + "_tick" : ""
        readonly property real boundaryX: root.tick <= 0 ? 0
                                          : root.tick >= root.chunkCount
                                          ? root.width
                                          : (root.chunkRight(root.tick - 1) + root.chunkLeft(root.tick)) / 2
        visible: root.tick >= 0 && root.chunkCount > 0
        z: 1
        width: 9
        height: root.height + 8
        x: Math.round(boundaryX - width / 2)
        y: -4
        Accessible.role: Accessible.Indicator
        Accessible.name: root.tickDescription

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 2
            height: parent.height
            color: root.palette.text
        }
        HoverHandler { id: tickHover }
        ToolTip.visible: tickHover.hovered && root.tickDescription.length > 0
        ToolTip.delay: Theme.shortToolTipDelay
        ToolTip.text: root.tickDescription
    }
}
