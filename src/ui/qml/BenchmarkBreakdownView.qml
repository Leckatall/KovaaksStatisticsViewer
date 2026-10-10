import QtQuick
import QtQuick.Controls

// The category/scenario breakdown as a table: one header row, then group rows
// (category / subcategory / Uncategorized) and scenario rows whose values sit in
// shared columns. Column widths are measured once over every scenario row of the
// whole tree, so a value lines up across categories, not just within one.
Item {
    id: root

    property var breakdown
    property var trackingVm
    // Kept for callers; the table layout is the same at every width (the name
    // column absorbs the difference and elides).
    property bool wide: true

    readonly property var _nodes: root.breakdown ? root.breakdown.nodes : []

    // Column geometry shared by every row (see BenchmarkBreakdownNode).
    readonly property real columnSpacing: 16
    readonly property real minimumNameWidth: 110
    readonly property var columns: _layoutColumns(width, _measured)

    implicitWidth: 600
    implicitHeight: rootColumn.implicitHeight

    // Never shown: lends its (inherited) font to the measurement, so the columns
    // follow the same font the rows render with, including scaled fonts.
    Label {
        id: fontProbe
        visible: false
        text: "0"
    }
    FontMetrics {
        id: metrics
        font: fontProbe.font
    }

    // Group rows indent by depth (toggle, swatch, name); a scenario's name starts
    // where its group's name does. BenchmarkBreakdownNode uses the same geometry.
    readonly property real indentStep: 16
    readonly property real toggleWidth: 22
    readonly property real swatchWidth: 10
    function groupNameIndent(depth) { return depth * indentStep + toggleWidth + 6 + swatchWidth + 6 }
    function scenarioNameIndent(depth) { return groupNameIndent(Math.max(0, depth - 1)) }

    function scenarioRecentText(node) {
        if (!node) return ""
        var text = String(node.recentAverageText)
        if (node.recentSampleCount > 0 && node.recentSampleCount < 5)
            text += qsTr(" (%1)").arg(node.recentSampleCount)
        return text
    }

    function scenarioNextText(node) {
        if (!node) return ""
        var tier = String(node.nextTierText || "")
        return tier.length > 0 ? tier + " " + node.nextThresholdText : String(node.nextThresholdText)
    }

    function scenarioStatusText(node) {
        if (!node) return ""
        var match = String(node.matchStateText || "")
        if (match.length > 0) return match
        return node.nextTierBlocker === true ? qsTr("Blocks next tier") : ""
    }

    readonly property var headers: ({
        name: qsTr("Scenario"),
        pb: qsTr("PB"),
        recent: qsTr("Recent"),
        rank: qsTr("Rank"),
        next: qsTr("Next tier"),
        status: qsTr("Status")
    })

    // Widest text per value column over every scenario in the tree (and the header).
    readonly property var _measured: {
        var widths = {
            name: metrics.advanceWidth(headers.name),
            pb: metrics.advanceWidth(headers.pb),
            recent: metrics.advanceWidth(headers.recent),
            rank: metrics.advanceWidth(headers.rank),
            next: metrics.advanceWidth(headers.next),
            status: metrics.advanceWidth(headers.status)
        }
        var walk = function (list, depth) {
            for (var i = 0; i < list.length; ++i) {
                var node = list[i]
                if (node.kind === "scenario") {
                    widths.name = Math.max(widths.name, root.scenarioNameIndent(depth)
                                                        + metrics.advanceWidth(String(node.name)))
                    widths.pb = Math.max(widths.pb, metrics.advanceWidth(String(node.personalBestText)))
                    widths.recent = Math.max(widths.recent, metrics.advanceWidth(scenarioRecentText(node)))
                    widths.rank = Math.max(widths.rank, metrics.advanceWidth(String(node.attainedTierText)))
                    widths.next = Math.max(widths.next, metrics.advanceWidth(scenarioNextText(node)))
                    widths.status = Math.max(widths.status, metrics.advanceWidth(scenarioStatusText(node)))
                }
                walk(node.children || [], depth + 1)
            }
        }
        walk(root._nodes, 0)
        for (var key in widths) widths[key] = Math.ceil(widths[key]) + 2
        // A column never narrows below its own heading.
        widths.minimum = {}
        for (var k in root.headers) widths.minimum[k] = Math.ceil(metrics.advanceWidth(root.headers[k])) + 2
        return widths
    }

    // Every column is as wide as its widest value, so values sit next to the names
    // they belong to on wide windows. When the table does not fit, columns give way
    // in order - the status column (it elides, with a tooltip), then the name column
    // down to its minimum, then the other value columns proportionally down to
    // their headings - and an over-long value elides rather than overflowing.
    function _layoutColumns(totalWidth, measured) {
        var gap = root.columnSpacing
        var order = ["pb", "recent", "rank", "next", "status"]
        var minimum = measured.minimum
        var widths = {}
        for (var i = 0; i < order.length; ++i)
            widths[order[i]] = order[i] === "status" ? Math.min(measured.status, 240) : measured[order[i]]
        var nameMinimum = Math.max(root.minimumNameWidth, minimum.name)
        var nameWidth = Math.max(nameMinimum, measured.name + 2 * gap)

        var used = nameWidth + gap * order.length
        for (i = 0; i < order.length; ++i) used += widths[order[i]]
        var excess = used - totalWidth

        if (excess > 0) {
            var statusCut = Math.min(excess, Math.max(0, widths.status - minimum.status))
            widths.status -= statusCut
            excess -= statusCut
        }
        if (excess > 0) {
            var nameCut = Math.min(excess, nameWidth - nameMinimum)
            nameWidth -= nameCut
            excess -= nameCut
        }
        if (excess > 0) {
            var slack = 0
            for (i = 0; i < order.length; ++i) slack += Math.max(0, widths[order[i]] - minimum[order[i]])
            if (slack > 0) {
                var share = Math.min(1, excess / slack)
                for (i = 0; i < order.length; ++i) {
                    var spare = Math.max(0, widths[order[i]] - minimum[order[i]])
                    widths[order[i]] = Math.floor(widths[order[i]] - spare * share)
                }
            }
        }

        var x = nameWidth + gap
        var cols = {nameWidth: nameWidth}
        for (i = 0; i < order.length; ++i) {
            cols[order[i]] = {x: x, width: widths[order[i]]}
            x += widths[order[i]] + gap
        }
        return cols
    }

    Column {
        id: rootColumn
        width: root.width
        spacing: 2

        // Column headings, aligned with the value columns of every row below.
        Item {
            id: headerRow
            objectName: "breakdownHeader"
            width: rootColumn.width
            height: headerName.implicitHeight + 8
            visible: root._nodes.length > 0

            component HeaderCell: Label {
                font.bold: true
                color: palette.placeholderText
                elide: Text.ElideRight
                y: 2
            }

            HeaderCell {
                id: headerName
                objectName: "breakdownHeader_name"
                x: 0
                width: root.columns.nameWidth
                text: root.headers.name
            }
            HeaderCell {
                objectName: "breakdownHeader_pb"
                x: root.columns.pb.x
                width: root.columns.pb.width
                horizontalAlignment: Text.AlignRight
                text: root.headers.pb
            }
            HeaderCell {
                objectName: "breakdownHeader_recent"
                x: root.columns.recent.x
                width: root.columns.recent.width
                horizontalAlignment: Text.AlignRight
                text: root.headers.recent
            }
            HeaderCell {
                objectName: "breakdownHeader_rank"
                x: root.columns.rank.x
                width: root.columns.rank.width
                text: root.headers.rank
            }
            HeaderCell {
                objectName: "breakdownHeader_next"
                x: root.columns.next.x
                width: root.columns.next.width
                horizontalAlignment: Text.AlignRight
                text: root.headers.next
            }
            HeaderCell {
                objectName: "breakdownHeader_status"
                x: root.columns.status.x
                width: root.columns.status.width
                text: root.headers.status
            }
            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: Math.min(parent.width, root.columns.status.x + root.columns.status.width)
                height: 1
                color: palette.mid
            }
        }

        Repeater {
            model: root._nodes

            delegate: BenchmarkBreakdownNode {
                required property var modelData
                width: rootColumn.width
                node: modelData
                trackingVm: root.trackingVm
                table: root
                depth: 0
            }
        }
    }
}
