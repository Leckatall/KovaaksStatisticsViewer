import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

// Every scenario of the selected benchmark in one grid laid out like the
// Benchmark Manager's editor table: Category, Subcategory (only when the benchmark
// has subcategories), Scenario, PB date, Plays, a segmented tier bar under one header
// cell per tier (which shows the PB and the recent average) and Next tier. Group cells
// span the rows of their scenarios and carry the group's tint; a row that keeps the
// benchmark from its next tier is marked in that tier's colour. Columns are measured
// over every row, so values line up under their headings. The tier headers show only
// their names, in the tier's readable colour, so a chunk need be no wider than the
// widest tier name. When the widths do not fit, the chunks first give up their padding
// around that name, then the scenario names elide, and the grid then scrolls
// horizontally inside its own frame. The end of the bar matters most, so an
// overflowing grid opens scrolled to its right end, and the identifying columns
// (Category, Subcategory, Scenario) stay pinned at the left while the rest scrolls
// under them.
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
    // Room on each side of the widest tier name in a comfortable chunk; it is the
    // first thing given up when the grid is short of width.
    readonly property real chunkPadding: 6
    // What is left around the widest name at the narrowest: slack for the chunks'
    // rounded edges, so a name is never cut off.
    readonly property real chunkSlack: 4
    readonly property real minimumScenarioWidth: 120

    // Which benchmark the grid shows (any stable key); a change scrolls back to the
    // right end and forgets the user's own scroll position.
    property string benchmarkKey

    readonly property real tableHeight: headerHeight + rows.length * rowHeight
    readonly property var columns: _layoutColumns(width, _measured)
    readonly property bool overflowing: columns.total > width + 0.5
    readonly property real barX: columns.progress.x + barInset
    readonly property real barWidth: Math.max(0, columns.progress.width - 2 * barInset)

    // --- horizontal scroll and pinned columns ----------------------------------------
    // Scroll rule: while the grid overflows it follows its right end (on first show,
    // on a resize, when the benchmark changes) until the user scrolls it somewhere
    // else. That position is kept, clamped to the new range on a resize, until the
    // benchmark changes, the grid stops overflowing, or the user scrolls back to the
    // right end, after which it follows the right end again.
    readonly property real maximumContentX: Math.max(0, columns.total - width)
    property bool userScrolled: false
    property bool _placingScroll: false

    // Category, Subcategory and Scenario stay put while the other columns scroll
    // under them, so a row stays identifiable at any scroll position. Not when that
    // would leave less than `minimumScrollingWidth` for the scrolling columns.
    readonly property real pinnedWidth: columns.scenario.x + columns.scenario.width
    readonly property real minimumScrollingWidth: 160
    readonly property bool pinning: overflowing && width - pinnedWidth >= minimumScrollingWidth
    readonly property real pinOffset: pinning ? Math.max(0, flick.contentX) : 0

    function _placeScroll() {
        if (!overflowing) userScrolled = false
        const target = !overflowing ? 0
                     : userScrolled ? Math.min(Math.max(flick.contentX, 0), maximumContentX)
                     : maximumContentX
        if (Math.abs(flick.contentX - target) < 0.01) return
        _placingScroll = true
        flick.contentX = target
        _placingScroll = false
    }
    function _noteUserScroll() {
        if (overflowing) userScrolled = Math.abs(flick.contentX - maximumContentX) > 0.5
    }
    onMaximumContentXChanged: _placeScroll()
    onOverflowingChanged: _placeScroll()
    onBenchmarkKeyChanged: {
        userScrolled = false
        _placeScroll()
    }
    Component.onCompleted: _placeScroll()

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

    // A category group whose rows have no subcategory group (scenarios straight under
    // the category, or Uncategorized) spans the Subcategory column too, rather than
    // leaving an empty cell beside it. Only meaningful while the grid shows that column.
    function spansSubcategory(group) {
        return hasSubcategories && group.spansSubcategory === true
    }

    function nextText(row) {
        if (!row) return ""
        const tier = String(row.nextTierText || "")
        return tier.length > 0 ? tier + " " + row.nextThresholdText : String(row.nextThresholdText)
    }

    function readableOn(color, background, fallback) {
        return Theme.readableOn(color, background, fallback)
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
        // Widest spanning category cell; it may use the Subcategory column's width too.
        let spanning = 0
        for (const group of groups) {
            const key = String(group.column)
            const need = boldMetrics.advanceWidth(String(group.name)) + rankChipSpace(group)
            if (spansSubcategory(group)) spanning = Math.max(spanning, need)
            else widths[key] = Math.max(widths[key], need)
        }
        for (const row of rows) {
            widths.scenario = Math.max(widths.scenario, markerSpace + metrics.advanceWidth(String(row.name)))
            widths.pbDate = Math.max(widths.pbDate, metrics.advanceWidth(String(row.personalBestDateText)))
            widths.plays = Math.max(widths.plays, metrics.advanceWidth(String(row.playCountText)))
            widths.next = Math.max(widths.next, metrics.advanceWidth(nextText(row)))
        }
        for (const key in widths) widths[key] = Math.ceil(widths[key] + 2 * cellPadding)
        if (spanning > 0)
            widths.category = Math.max(widths.category, Math.ceil(spanning + 2 * cellPadding) - widths.subcategory)
        // A tier header shows only its name: a chunk is comfortable with some padding
        // around the widest name, and never narrower than that name.
        let tierName = 0
        for (const tier of tiers)
            tierName = Math.max(tierName, boldMetrics.advanceWidth(String(tier.name)))
        widths.chunk = Math.ceil(tierName + 2 * chunkPadding)
        widths.chunkMinimum = Math.ceil(tierName + chunkSlack)
        widths.scenarioMinimum = Math.ceil(Math.max(minimumScenarioWidth, scenarioHeader + 2 * cellPadding))
        return widths
    }

    // Every column is as wide as its widest value; the bar takes the remaining width,
    // between one comfortable chunk per tier and a cap that keeps it readable on very
    // wide windows. Too narrow: the chunks give up their padding around the tier names
    // first (down to the widest name), then the scenario names give way (eliding),
    // then the grid scrolls.
    function _layoutColumns(totalWidth, measured) {
        const count = Math.max(1, tiers.length)
        const category = measured.category
        const subcategory = hasSubcategories ? measured.subcategory : 0
        let scenario = measured.scenario
        const progressMinimum = count * measured.chunkMinimum + 2 * barInset
        const progressComfortable = count * measured.chunk + 2 * barInset
        const progressMaximum = count * Math.max(measured.chunk, maximumChunkWidth) + 2 * barInset
        const fixed = category + subcategory + scenario + measured.pbDate + measured.plays + measured.next
        let progress = Math.min(progressMaximum, Math.max(progressComfortable, totalWidth - fixed))
        let excess = fixed + progress - totalWidth
        if (excess > 0)
            progress -= Math.min(excess, progress - progressMinimum)
        excess = fixed + progress - totalWidth
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
        place("pbDate", measured.pbDate)
        place("plays", measured.plays)
        place("progress", progress)
        place("next", measured.next)
        cols.total = x
        return cols
    }

    component HeaderCell: GridHeaderCell {
        y: 0
        height: root.headerHeight
        labelPadding: root.cellPadding
        Accessible.role: Accessible.ColumnHeader
        Accessible.name: text
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

        // Only a move the user makes (dragging, flicking, the wheel, the scroll bar)
        // counts; the grid's own moves and the Flickable's bounds fix-ups do not. It is
        // kept unless it ends back on the right end.
        onContentXChanged: {
            if (root._placingScroll || !root.overflowing) return
            if (moving || horizontalBar.pressed) root._noteUserScroll()
        }
        onMovementEnded: root._noteUserScroll()

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
            onPressedChanged: if (!pressed) root._noteUserScroll()
        }

        Item {
            id: table
            objectName: "progressGridTable"
            width: root.columns.total
            height: root.tableHeight

            // --- header ----------------------------------------------------------
            HeaderCell {
                objectName: "progressHeader_category"
                z: 2
                x: root.columns.category.x + root.pinOffset
                width: root.columns.category.width
                text: root.headers.category
            }
            HeaderCell {
                objectName: "progressHeader_subcategory"
                z: 2
                visible: root.hasSubcategories
                x: root.columns.subcategory.x + root.pinOffset
                width: root.columns.subcategory.width
                text: root.headers.subcategory
            }
            HeaderCell {
                objectName: "progressHeader_scenario"
                z: 2
                x: root.columns.scenario.x + root.pinOffset
                width: root.columns.scenario.width
                text: root.headers.scenario
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
                objectName: "progressHeader_progress"
                x: root.columns.progress.x
                width: root.columns.progress.width
                Accessible.name: root.headers.progress
            }
            HeaderCell {
                objectName: "progressHeader_next"
                x: root.columns.next.x
                width: root.columns.next.width
                alignment: Text.AlignRight
                text: root.headers.next
            }

            // One header cell per tier, over its chunk of every bar below: the tier's
            // name in its readable colour, underlined in the tier's bar colour. No
            // swatch: in the viewer it would suggest the colour can be edited here.
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
                    Label {
                        objectName: "progressTierHeader_" + tierHeader.index + "_name"
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -1
                        anchors.horizontalCenterOffset: (tierHeader.leadingInset - tierHeader.trailingInset) / 2
                        font.bold: true
                        text: String(tierHeader.modelData.name)
                        color: root.readableOn(String(tierHeader.modelData.barColor || ""),
                                               root.palette.button, root.palette.buttonText)
                    }
                }
            }

            // --- scenario rows -----------------------------------------------------
            Repeater {
                model: root.rows
                delegate: ProgressGridRow { grid: root }
            }

            // --- group cells, over the rows they span ------------------------------
            Repeater {
                model: root.groups
                delegate: ProgressGridGroupCell { grid: root }
            }

            // While columns pass under the pinned ones, a soft edge marks where.
            Rectangle {
                objectName: "progressGridPinEdge"
                z: 4
                visible: root.pinning && root.pinOffset > 0.5
                x: root.pinOffset + root.pinnedWidth
                width: 6
                height: root.tableHeight
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Qt.alpha(root.palette.shadow, 0.45) }
                    GradientStop { position: 1; color: Qt.alpha(root.palette.shadow, 0) }
                }
            }
        }
    }
}
