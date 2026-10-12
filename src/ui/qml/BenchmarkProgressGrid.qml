import QtQuick
import QtQuick.Controls

// Every scenario of the selected benchmark in one grid laid out like the
// Benchmark Manager's editor table: Category, Subcategory (only when the benchmark
// has subcategories), Scenario, a segmented tier bar under one header cell per tier
// (which shows the PB and the recent average), PB date, Plays and Next tier. Group cells span the rows of their scenarios and carry
// the group's tint; a row that keeps the benchmark from its next tier is marked in
// that tier's colour. Columns are measured over every row, so values line up under
// their headings. When the minimum widths do not fit, the scenario names elide
// first and the grid then scrolls horizontally inside its own frame.
Item {
    id: root

    // BenchmarkProgressGridModel: tiers, rows, groups, hasSubcategories.
    property var grid

    readonly property var tiers: grid && grid.tiers ? grid.tiers : []
    readonly property var rows: grid && grid.rows ? grid.rows : []
    readonly property var groups: grid && grid.groups ? grid.groups : []
    readonly property bool hasSubcategories: grid ? grid.hasSubcategories === true : false

    readonly property int rowHeight: 30
    readonly property int headerHeight: 30
    readonly property real cellPadding: 8
    // The bar sits this far inside its column; the tier headers use the same inset.
    readonly property real barInset: 8
    readonly property real barHeight: 16
    // Room in front of every scenario name for the blocker marker, so names align.
    readonly property real markerSpace: 16
    readonly property real maximumChunkWidth: 160
    readonly property real minimumScenarioWidth: 120

    readonly property real tableHeight: headerHeight + rows.length * rowHeight
    readonly property var columns: _layoutColumns(width, _measured)
    readonly property bool overflowing: columns.total > width + 0.5
    readonly property real barX: columns.progress.x + barInset
    readonly property real barWidth: Math.max(0, columns.progress.width - 2 * barInset)

    // The narrowest layout, independent of the width it is given.
    implicitWidth: _layoutColumns(0, _measured).total
    implicitHeight: rows.length > 0 ? tableHeight + (overflowing ? horizontalBar.implicitHeight : 0) : 0
    visible: rows.length > 0

    readonly property var headers: ({
        category: qsTr("Category"),
        subcategory: qsTr("Subcategory"),
        scenario: qsTr("Scenario"),
        progress: qsTr("Progress"),
        pbDate: qsTr("PB date"),
        plays: qsTr("Plays"),
        next: qsTr("Next tier")
    })

    function nextText(row) {
        if (!row) return ""
        const tier = String(row.nextTierText || "")
        return tier.length > 0 ? tier + " " + row.nextThresholdText : String(row.nextThresholdText)
    }

    // The left edge of tier header `index` inside the bar: the same equal shares the
    // bars' chunk boundaries use (TierBar.chunkLeft/chunkRight, without the gap).
    function tierEdge(index) {
        return tiers.length > 0 ? Math.round(index * barWidth / tiers.length) : 0
    }

    // --- group rank chips ---------------------------------------------------------
    readonly property real rankChipPadding: 6
    readonly property real rankChipSpacing: 6
    // The width a group's rank chip adds beside its name (none without a rank).
    function rankChipSpace(group) {
        const text = String(group.rankText || "")
        return text.length > 0 ? rankChipSpacing + 2 * rankChipPadding + chipMetrics.advanceWidth(text) + 2 : 0
    }
    Label {
        id: chipProbe
        visible: false
        font.pixelSize: 11
        text: "0"
    }
    FontMetrics {
        id: chipMetrics
        font: chipProbe.font
    }

    // Never shown: lend their (inherited) fonts to the measurement, so the columns
    // follow the fonts the cells render with, including scaled fonts.
    Label {
        id: fontProbe
        visible: false
        text: "0"
    }
    Label {
        id: boldProbe
        visible: false
        font.bold: true
        text: "0"
    }
    FontMetrics {
        id: metrics
        font: fontProbe.font
    }
    FontMetrics {
        id: boldMetrics
        font: boldProbe.font
    }

    // Widest content per column over the header and every row, padding included.
    readonly property var _measured: {
        // advanceWidth() is a call, not a property: read the fonts so a font change
        // re-measures.
        void metrics.font
        void boldMetrics.font
        const widths = {
            category: boldMetrics.advanceWidth(headers.category),
            subcategory: boldMetrics.advanceWidth(headers.subcategory),
            scenario: boldMetrics.advanceWidth(headers.scenario),
            pbDate: boldMetrics.advanceWidth(headers.pbDate),
            plays: boldMetrics.advanceWidth(headers.plays),
            next: boldMetrics.advanceWidth(headers.next)
        }
        const scenarioHeader = widths.scenario
        for (const group of groups) {
            const key = String(group.column)
            widths[key] = Math.max(widths[key], boldMetrics.advanceWidth(String(group.name)) + rankChipSpace(group))
        }
        for (const row of rows) {
            widths.scenario = Math.max(widths.scenario, markerSpace + metrics.advanceWidth(String(row.name)))
            widths.pbDate = Math.max(widths.pbDate, metrics.advanceWidth(String(row.personalBestDateText)))
            widths.plays = Math.max(widths.plays, metrics.advanceWidth(String(row.playCountText)))
            widths.next = Math.max(widths.next, metrics.advanceWidth(nextText(row)))
        }
        for (const key in widths) widths[key] = Math.ceil(widths[key] + 2 * cellPadding)
        // A tier header shows its swatch and name side by side; no chunk is narrower.
        let chunk = 0
        for (const tier of tiers)
            chunk = Math.max(chunk, 14 + 6 + boldMetrics.advanceWidth(String(tier.name)) + 2 * 6)
        widths.chunk = Math.ceil(chunk)
        widths.scenarioMinimum = Math.ceil(Math.max(minimumScenarioWidth, scenarioHeader + 2 * cellPadding))
        return widths
    }

    // Every column is as wide as its widest value; the bar takes the remaining width,
    // between one minimum-width chunk per tier and a cap that keeps it readable on
    // very wide windows. Too narrow: the scenario names give way first (eliding),
    // then the grid scrolls.
    function _layoutColumns(totalWidth, measured) {
        const count = Math.max(1, tiers.length)
        const category = measured.category
        const subcategory = hasSubcategories ? measured.subcategory : 0
        let scenario = measured.scenario
        const progressMinimum = count * measured.chunk + 2 * barInset
        const progressMaximum = count * Math.max(measured.chunk, maximumChunkWidth) + 2 * barInset
        const fixed = category + subcategory + scenario + measured.pbDate + measured.plays + measured.next
        const progress = Math.min(progressMaximum, Math.max(progressMinimum, totalWidth - fixed))
        const excess = fixed + progress - totalWidth
        if (excess > 0)
            scenario -= Math.min(excess, Math.max(0, scenario - measured.scenarioMinimum))

        let x = 0
        const cols = {}
        const place = function (key, width) {
            cols[key] = {x: x, width: width}
            x += width
        }
        place("category", category)
        place("subcategory", subcategory)
        place("scenario", scenario)
        place("progress", progress)
        place("pbDate", measured.pbDate)
        place("plays", measured.plays)
        place("next", measured.next)
        cols.total = x
        return cols
    }

    component HeaderCell: Rectangle {
        id: headerCell
        property string text
        property int alignment: Text.AlignLeft
        y: 0
        height: root.headerHeight
        color: root.palette.button
        border.color: root.palette.mid
        Accessible.role: Accessible.ColumnHeader
        Accessible.name: text

        Label {
            anchors.fill: parent
            anchors.leftMargin: root.cellPadding
            anchors.rightMargin: root.cellPadding
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: headerCell.alignment
            elide: Text.ElideRight
            font.bold: true
            text: headerCell.text
        }
    }

    component ValueCell: Label {
        required property real columnX
        required property real columnWidth
        x: columnX + root.cellPadding
        width: Math.max(0, columnWidth - 2 * root.cellPadding)
        height: root.rowHeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        color: text === "—" ? root.palette.placeholderText : root.palette.text
    }

    Flickable {
        id: flick
        objectName: "progressGridFlick"
        width: root.width
        height: root.tableHeight
        contentWidth: root.columns.total
        contentHeight: root.tableHeight
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: root.overflowing
        clip: true

        // Below the table rather than over its last row, and only while it overflows.
        ScrollBar.horizontal: ScrollBar {
            id: horizontalBar
            objectName: "progressGridScrollBar"
            parent: root
            x: 0
            y: flick.height
            width: root.width
            visible: root.overflowing
            policy: root.overflowing ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        Item {
            id: table
            objectName: "progressGridTable"
            width: root.columns.total
            height: root.tableHeight

            // --- header ----------------------------------------------------------
            HeaderCell {
                objectName: "progressHeader_category"
                x: root.columns.category.x
                width: root.columns.category.width
                text: root.headers.category
            }
            HeaderCell {
                objectName: "progressHeader_subcategory"
                visible: root.hasSubcategories
                x: root.columns.subcategory.x
                width: root.columns.subcategory.width
                text: root.headers.subcategory
            }
            HeaderCell {
                objectName: "progressHeader_scenario"
                x: root.columns.scenario.x
                width: root.columns.scenario.width
                text: root.headers.scenario
            }
            HeaderCell {
                objectName: "progressHeader_progress"
                x: root.columns.progress.x
                width: root.columns.progress.width
                Accessible.name: root.headers.progress
            }
            HeaderCell {
                objectName: "progressHeader_pbDate"
                x: root.columns.pbDate.x
                width: root.columns.pbDate.width
                alignment: Text.AlignRight
                text: root.headers.pbDate
            }
            HeaderCell {
                objectName: "progressHeader_plays"
                x: root.columns.plays.x
                width: root.columns.plays.width
                alignment: Text.AlignRight
                text: root.headers.plays
            }
            HeaderCell {
                objectName: "progressHeader_next"
                x: root.columns.next.x
                width: root.columns.next.width
                alignment: Text.AlignRight
                text: root.headers.next
            }

            // One header cell per tier, over its chunk of every bar below, in the
            // editor's tier-header look: swatch and name, underlined in the tier colour.
            // The first and last cells also cover the bar's inset, so the inset does not
            // read as a column of its own; every name stays centred over its chunk.
            Repeater {
                model: root.tiers
                delegate: Rectangle {
                    id: tierHeader
                    required property var modelData
                    required property int index
                    readonly property real leadingInset: index === 0 ? root.barInset : 0
                    readonly property real trailingInset: index === root.tiers.length - 1 ? root.barInset : 0
                    objectName: "progressTierHeader_" + index
                    x: root.barX + root.tierEdge(index) - leadingInset
                    y: 0
                    width: root.tierEdge(index + 1) - root.tierEdge(index) + leadingInset + trailingInset
                    height: root.headerHeight
                    color: root.palette.button
                    border.color: root.palette.mid
                    Accessible.role: Accessible.ColumnHeader
                    Accessible.name: String(modelData.name)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 4
                        color: String(tierHeader.modelData.barColor || "")
                    }
                    Row {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -1
                        anchors.horizontalCenterOffset: (tierHeader.leadingInset - tierHeader.trailingInset) / 2
                        spacing: 6
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            radius: 3
                            color: String(tierHeader.modelData.barColor || "")
                            border.color: root.palette.mid
                        }
                        Label {
                            objectName: "progressTierHeader_" + tierHeader.index + "_name"
                            anchors.verticalCenter: parent.verticalCenter
                            font.bold: true
                            text: String(tierHeader.modelData.name)
                        }
                    }
                }
            }

            // --- scenario rows -----------------------------------------------------
            Repeater {
                model: root.rows
                delegate: Item {
                    id: rowItem
                    required property var modelData
                    required property int index

                    objectName: "progressRow_" + modelData.entryId
                    x: 0
                    y: root.headerHeight + index * root.rowHeight
                    width: root.columns.total
                    height: root.rowHeight
                    Accessible.role: Accessible.Row
                    Accessible.name: String(modelData.name)

                    Rectangle {
                        anchors.fill: parent
                        color: root.palette.base
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: root.palette.mid
                    }
                    Repeater {
                        model: ["category", "subcategory", "scenario", "progress", "pbDate", "plays", "next"]
                        delegate: Rectangle {
                            required property string modelData
                            visible: root.columns[modelData].width > 0
                            x: root.columns[modelData].x + root.columns[modelData].width - 1
                            width: 1
                            height: rowItem.height
                            color: root.palette.mid
                        }
                    }

                    // Blocker marker, in the next tier's colour.
                    Item {
                        objectName: rowItem.modelData.blocker === true ? "progressRow_" + rowItem.modelData.entryId + "_blocker" : ""
                        visible: rowItem.modelData.blocker === true
                        x: root.columns.scenario.x + root.cellPadding - 4
                        width: root.markerSpace
                        height: rowItem.height
                        Accessible.role: Accessible.Indicator
                        Accessible.name: String(rowItem.modelData.blockerText || "")

                        Rectangle {
                            anchors.centerIn: parent
                            width: 9
                            height: 9
                            rotation: 45
                            color: String(rowItem.modelData.blockerColor || "") || root.palette.highlight
                        }
                        HoverHandler { id: blockerHover }
                        ToolTip.visible: blockerHover.hovered
                        ToolTip.delay: 300
                        ToolTip.text: String(rowItem.modelData.blockerText || "")
                    }

                    Label {
                        id: nameLabel
                        objectName: "progressRow_" + rowItem.modelData.entryId + "_name"
                        x: root.columns.scenario.x + root.cellPadding + root.markerSpace
                        width: Math.max(0, root.columns.scenario.width - 2 * root.cellPadding - root.markerSpace)
                        height: rowItem.height
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: String(rowItem.modelData.name)

                        HoverHandler { id: nameHover }
                        ToolTip.visible: nameHover.hovered && nameLabel.truncated
                        ToolTip.delay: 400
                        ToolTip.text: nameLabel.text
                    }

                    TierBar {
                        objectName: "progressRow_" + rowItem.modelData.entryId + "_bar"
                        x: root.barX
                        y: Math.round((rowItem.height - height) / 2)
                        width: root.barWidth
                        height: root.barHeight
                        marker: rowItem.modelData.recentPosition !== undefined
                                ? Number(rowItem.modelData.recentPosition) : -1
                        markerDescription: String(rowItem.modelData.recentDescription || "")
                        tiers: root.tiers
                        fills: rowItem.modelData.fills || []
                        labels: rowItem.modelData.thresholdTexts || []
                        valueText: rowItem.modelData.personalBestText !== "—"
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
                        columnX: root.columns.pbDate.x
                        columnWidth: root.columns.pbDate.width
                        text: String(rowItem.modelData.personalBestDateText)
                        Accessible.name: qsTr("PB date %1").arg(text)
                    }
                    ValueCell {
                        objectName: "progressRow_" + rowItem.modelData.entryId + "_playCount"
                        columnX: root.columns.plays.x
                        columnWidth: root.columns.plays.width
                        text: String(rowItem.modelData.playCountText)
                        Accessible.name: qsTr("Plays %1").arg(text)
                    }
                    Row {
                        objectName: "progressRow_" + rowItem.modelData.entryId + "_next"
                        x: root.columns.next.x + root.cellPadding
                        width: Math.max(0, root.columns.next.width - 2 * root.cellPadding)
                        height: rowItem.height
                        layoutDirection: Qt.RightToLeft
                        spacing: 4
                        clip: true
                        // Read by tests and assistive tech as one value.
                        readonly property string text: root.nextText(rowItem.modelData)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("Next tier %1").arg(text)

                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: String(rowItem.modelData.nextThresholdText)
                            color: String(rowItem.modelData.nextTierText || "").length > 0
                                   ? root.palette.text : root.palette.placeholderText
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: text.length > 0
                            text: String(rowItem.modelData.nextTierText || "")
                            color: String(rowItem.modelData.nextTierColor || "").length > 0
                                   ? rowItem.modelData.nextTierColor : root.palette.text
                        }
                    }
                }
            }

            // --- group cells, over the rows they span ------------------------------
            Repeater {
                model: root.groups
                delegate: Rectangle {
                    id: groupCell
                    required property var modelData
                    readonly property var column: root.columns[String(modelData.column)]
                    readonly property string tint: String(modelData.color || "")

                    objectName: "progressGroup_" + (String(modelData.id).length > 0 ? modelData.id : "uncategorized")
                    visible: column.width > 0
                    x: column.x
                    y: root.headerHeight + Number(modelData.start) * root.rowHeight
                    width: column.width
                    height: Number(modelData.span) * root.rowHeight
                    color: root.palette.base
                    Accessible.role: Accessible.Cell
                    Accessible.name: String(modelData.name)

                    Rectangle {
                        anchors.fill: parent
                        visible: groupCell.tint.length > 0
                        color: groupCell.tint.length > 0 ? Qt.alpha(groupCell.tint, 0.22) : "transparent"
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: root.palette.mid
                    }
                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        width: 1
                        color: root.palette.mid
                    }
                    Row {
                        x: root.cellPadding
                        width: Math.max(0, groupCell.width - 2 * root.cellPadding)
                        height: root.rowHeight
                        spacing: root.rankChipSpacing

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
                            width: rankLabel.implicitWidth + 2 * root.rankChipPadding
                            height: rankLabel.implicitHeight + 4
                            radius: height / 2
                            color: tierColor.length > 0 ? Qt.alpha(tierColor, 0.28) : root.palette.base
                            border.color: tierColor.length > 0 ? tierColor : root.palette.mid
                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("%1 rank: %2").arg(String(groupCell.modelData.name)).arg(rankLabel.text)

                            Label {
                                id: rankLabel
                                anchors.centerIn: parent
                                font.pixelSize: 11
                                text: String(groupCell.modelData.rankText || "")
                                color: String(groupCell.modelData.rankColor || "").length > 0
                                       ? groupCell.modelData.rankColor : root.palette.placeholderText
                            }
                        }
                    }
                }
            }
        }
    }
}
