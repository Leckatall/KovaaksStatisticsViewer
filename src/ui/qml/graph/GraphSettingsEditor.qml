pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import KovaaksStatsViewer
import "GraphCatalog.js" as GraphCatalog

// The Graphs settings page's editor for one graph: Data & range, Lines & style and Axes, each only when the
// graph has something to put in it. Every control edits the settings visit's working copy through
// settingsVm; numbers and dates go to the view model as typed and are read there, in the app locale.
Item {
    id: root
    objectName: "graphSettingsEditor"

    required property var settingsVm
    // One of run, dailyPlaytime, scenarioHistory, benchmarkRank, benchmarkRecentRank, benchmarkPlaytime.
    required property string graphKey
    // Visibility is a saved layout preference, not part of the visit's working copy.
    property var visualSettings: null

    readonly property bool graphShown: visualSettings ? visualSettings.isGraphVisible(graphKey) : true

    readonly property alias colorDialog: seriesColorDialog
    readonly property alias scroller: editorScroll

    readonly property var draft: (settingsVm.graphDrafts || {})[graphKey] || ({})
    readonly property var limits: settingsVm.graphLimits || ({})
    readonly property string graphName: GraphCatalog.nameOf(graphKey)
    readonly property bool offersRange: draft.hasRange === true
    readonly property bool offersLatestRuns: draft.hasLatestRuns === true
    readonly property bool offersWindow: draft.hasWindow === true
    readonly property var axisRows: draft.axes || []
    readonly property var seriesRows: draft.series || []
    // The Repeaters are keyed by these, which change only when the graph's axes or series do: every
    // keystroke republishes the drafts, and a row rebuilt from them would take the field being typed in.
    readonly property string axisKeys: axisRows.map(r => r.key).join(",")
    readonly property string seriesKeys: seriesRows.map(r => r.key).join(",")
    readonly property bool isRunGraph: graphKey === "run"
    readonly property bool wide: editorScroll.availableWidth >= 560
    // Each group's visibility is bound to this list, so the sidebar never lists a hidden section.
    readonly property var sections: {
        const result = []
        if (offersRange || offersWindow) result.push("dataRange")
        result.push("seriesStyle")
        if (isRunGraph || axisRows.length > 0) result.push("axes")
        return result
    }
    readonly property var sectionGroups: ({
        dataRange: dataRangeGroup,
        seriesStyle: seriesStyleGroup,
        axes: axesGroup
    })
    readonly property string sectionInView: {
        const flick = editorScroll.contentItem as Flickable
        const y = flick.contentY
        const maximum = Math.max(0, flick.contentHeight - flick.height)
        if (maximum > 0 && y >= maximum - 1) return sections[sections.length - 1]
        let current = sections[0]
        // Reads each group's own y, not mapToItem(): that notifies nothing, so this binding would keep
        // the positions from before the layout's first polish, when every group is still at 0.
        for (const section of sections) {
            const group = sectionGroups[section]
            if (group.y <= y + group.heading.height)
                current = section
        }
        return current
    }
    function scrollToSection(group) {
        const heading = sectionGroups[group] ? sectionGroups[group].heading : null
        if (!heading || !sections.includes(group)) return
        const flick = editorScroll.contentItem as Flickable
        flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height,
            heading.mapToItem(flick.contentItem, 0, 0).y))
    }

    readonly property var rangeKinds: {
        const kinds = [
            {kind: "allHistory", text: GraphCatalog.allHistoryText(offersLatestRuns)},
            {kind: "recentDays", text: qsTr("Recent days")}
        ]
        if (offersLatestRuns) kinds.push({kind: "latestRuns", text: qsTr("Latest runs")})
        kinds.push({kind: "customDates", text: qsTr("Custom dates")})
        return kinds
    }
    readonly property int rangeIndex: Math.max(0, rangeKinds.findIndex(k => k.kind === draft.rangeKind))

    readonly property var fitEntries: [
        {text: qsTr("Fit visible data"), mode: "fit"},
        {text: qsTr("Start at zero"), mode: "zero"},
        {text: qsTr("Fixed bounds"), mode: "fixed"}
    ]

    function num(value) {
        return GraphCatalog.wholeNumberText(value)
    }

    function keyList(keys) {
        return keys === "" ? [] : keys.split(",")
    }

    function rowFor(rows, key) {
        return rows.find(r => r.key === key) || ({key: key})
    }

    function decimal(value) {
        return Number(value).toLocaleString(Qt.locale(), "g", 6)
    }

    function daysText(count) {
        return count === 1 ? qsTr("1 day") : qsTr("%1 days").arg(num(count))
    }

    function edit(group, target, field, text) {
        root.settingsVm.setGraphField(root.graphKey, group, target, field, text)
    }

    // ---- Issues ---------------------------------------------------------------------------------------

    // This graph's live issues by group|target|field, so each error label is one lookup.
    readonly property var issuesByField: {
        const found = {}
        for (const i of root.settingsVm.draftIssues || [])
            if (i.graph === root.graphKey && !(i.group + "|" + i.target + "|" + i.field in found))
                found[i.group + "|" + i.target + "|" + i.field] = i
        return found
    }

    function issueFor(group, target, field) {
        return root.issuesByField[group + "|" + target + "|" + field] || null
    }

    function errorFor(group, target, field) {
        const issue = issueFor(group, target, field)
        return issue ? messageFor(issue) : ""
    }

    function messageFor(issue) {
        const L = root.limits
        switch (issue.field) {
        case "recentDays":
            return qsTr("Enter a whole number of days from 1 to %1.").arg(num(L.maxRecentDays))
        case "latestRuns":
            return qsTr("Enter a whole number of runs from 1 to %1.").arg(num(L.maxLatestRuns))
        case "averagingWindow":
            return qsTr("Enter a whole number of days from 1 to %1.").arg(num(L.maxAveragingWindow))
        case "startDate":
        case "endDate":
            if (issue.code === "reversedDateRange") return qsTr("The end date must not be before the start date.")
            if (issue.code === "invalidDate")
                return qsTr("Dates run from %1 to %2.").arg(L.earliestDateText).arg(L.latestDateText)
            return qsTr("Enter a date as %1.").arg(L.dateFormat)
        case "width":
            return qsTr("Enter a width from %1 to %2.").arg(decimal(L.minWidth)).arg(decimal(L.maxWidth))
        case "fixedMin":
        case "fixedMax":
            if (issue.code === "axisBoundOutOfRange")
                return qsTr("Bounds must be between -%1 and %1.").arg(num(L.maxBound))
            if (issue.code === "fixedBoundsNotIncreasing") return qsTr("The maximum must be greater than the minimum.")
            return qsTr("Enter a number.")
        }
        return qsTr("This setting is not valid.")
    }

    // ---- Focus and scrolling ----------------------------------------------------------------------------

    function ensureVisible(item) {
        GraphCatalog.ensureVisible(editorScroll.contentItem as Flickable, item, 8)
    }

    // Moves keyboard focus to the control an issue concerns; true when there was one to move to.
    function focusIssue(issue) {
        const target = GraphCatalog.findByObjectName(content, GraphCatalog.controlName(issue))
                    || GraphCatalog.findByObjectName(content, "graphGroup_" + issue.group)
        if (!target) return false
        ensureVisible(target)
        target.forceActiveFocus()
        return true
    }

    function firstTabStop(item) {
        if (!item.visible || !item.enabled) return null
        if (item.activeFocusOnTab) return item
        for (const child of item.children) {
            const found = firstTabStop(child)
            if (found) return found
        }
        return null
    }

    function focusFirstControl() {
        const first = firstTabStop(content)
        if (!first) return false
        const flick = editorScroll.contentItem as Flickable
        first.forceActiveFocus(Qt.TabFocusReason)
        // Set after focusing, which scrolls too: the page opens at its top, with the heading row and
        // Reset to defaults above this field in view.
        flick.contentY = 0
        return true
    }

    // Tabbing to a control that is scrolled out of view brings it into view.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            const focused = root.Window.window ? root.Window.window.activeFocusItem : null
            if (focused && GraphCatalog.isWithin(focused, content)) root.ensureVisible(focused)
        }
    }

    function pickColor(row) {
        seriesColorDialog.seriesKey = row.key
        seriesColorDialog.selectedColor = row.color ? row.color : editorScroll.palette.text
        seriesColorDialog.open()
    }

    ColorDialog {
        id: seriesColorDialog
        property string seriesKey
        options: ColorDialog.ShowAlphaChannel
        onAccepted: root.settingsVm.setSeriesColor(root.graphKey, seriesKey, selectedColor)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Rectangle {
            objectName: "graphVisibilityCard"
            Layout.fillWidth: true
            implicitHeight: cardRow.implicitHeight + 28
            radius: 6
            color: root.graphShown ? Theme.graphShownSurface : Theme.graphHiddenSurface
            border.color: root.graphShown ? Theme.graphShownBorder : Theme.graphHiddenBorder

            Rectangle {
                objectName: "graphVisibilityEdge"
                width: 4
                radius: 2
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 1
                color: root.graphShown ? visibilitySwitch.palette.accent : Theme.graphHiddenEdge
            }
            RowLayout {
                id: cardRow
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Image {
                    objectName: "graphVisibilityIcon"
                    source: root.graphShown ? "../icons/eye.svg" : "../icons/eye-off.svg"
                    sourceSize.width: 26
                    sourceSize.height: 26
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    Accessible.ignored: true
                }
                Label {
                    objectName: "graphPageTitle"
                    Layout.fillWidth: true
                    text: root.graphName
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Switch {
                    id: visibilitySwitch
                    objectName: "graphVisibleSwitch"
                    visible: root.visualSettings !== null
                    text: qsTr("Show graph")
                    checked: root.graphShown
                    Accessible.name: qsTr("Show %1 graph").arg(root.graphName)
                    Accessible.description: (root.graphShown ? qsTr("Shown in %1") : qsTr("Hidden from %1"))
                        .arg(GraphCatalog.workspaceOf(root.graphKey))
                    onToggled: root.visualSettings.setGraphVisible(root.graphKey, checked)
                }
            }
        }
        OverflowScrollView {
            id: editorScroll
            objectName: "graphEditorScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true

            // The button shares the first section's heading row. It comes first so it is tabbed to before
            // the fields, and stays outside `content` so opening a graph still focuses its first field.
            Item {
                width: content.width
                implicitHeight: content.implicitHeight

                Button {
                    id: resetButton
                    objectName: "graphResetButton"
                    readonly property string scopeText: root.isRunGraph
                        ? qsTr("Stages Run graph's default line style. Your lines, axes and visibility are kept; use Restore default lines for lines and axes.")
                        : qsTr("Stages this graph's defaults for every section. Visibility is kept. Save or Cancel decides.")
                    anchors.right: parent.right
                    z: 1
                    text: qsTr("Reset to defaults")
                    flat: true
                    Accessible.name: qsTr("Reset %1 to its defaults").arg(root.graphName)
                    Accessible.description: scopeText
                    ToolTip.text: scopeText
                    ToolTip.visible: hovered
                    onClicked: root.settingsVm.resetGraph(root.graphKey)
                }

            ColumnLayout {
                id: content
                objectName: "graphSettingsContent"
                // The bar is drawn over the view's right edge, so its width is kept clear whether or not it shows.
                width: editorScroll.availableWidth - editorScroll.ScrollBar.vertical.width - 4
                spacing: 16

                // ---- Data & range ------------------------------------------------------------------------------
                GraphSettingsGroup {
                    id: dataRangeGroup
                    groupName: "dataRange"
                    title: qsTr("Data & range")
                    headerMinimumHeight: root.sections[0] === groupName ? resetButton.implicitHeight : 0
                    visible: root.sections.includes("dataRange")

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.wide ? 2 : 1
                        columnSpacing: 16
                        rowSpacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: 2
                            visible: root.offersRange

                            Label { text: qsTr("Visible range") }
                            ComboBox {
                                id: rangeCombo
                                objectName: "graphField_dataRange_range"
                                implicitContentWidthPolicy: ComboBox.WidestText
                                Layout.fillWidth: true
                                Layout.maximumWidth: Math.max(implicitWidth, rangeMetrics.height * 16)
                                FontMetrics { id: rangeMetrics; font: rangeCombo.font }
                                readonly property var entries: root.rangeKinds
                                model: entries
                                textRole: "text"
                                currentIndex: root.rangeIndex
                                Accessible.name: qsTr("Visible range")
                                onActivated: index => {
                                    root.settingsVm.setGraphRangeKind(root.graphKey, entries[index].kind)
                                    rangeCombo.currentIndex = Qt.binding(() => root.rangeIndex)
                                }
                            }
                        }

                        GraphNumberField {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            visible: root.offersWindow
                            fieldName: "graphField_dataRange_averagingWindow"
                            graphKey: root.graphKey
                            label: qsTr("Rolling average")
                            unit: qsTr("days")
                            hint: qsTr("Whole days, 1 to %1. 1 shows daily totals.").arg(root.num(root.limits.maxAveragingWindow))
                            shown: root.draft.windowText || ""
                            errorText: root.errorFor("dataRange", "", "averagingWindow")
                            onEdited: text => root.edit("dataRange", "", "averagingWindow", text)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.offersRange

                        GraphNumberField {
                            Layout.fillWidth: true
                            visible: root.draft.rangeKind === "recentDays"
                            fieldName: "graphField_dataRange_recentDays"
                            graphKey: root.graphKey
                            label: qsTr("Number of days")
                            unit: qsTr("days")
                            hint: qsTr("Whole days, 1 to %1.").arg(root.num(root.limits.maxRecentDays))
                            shown: root.draft.rangeKind === "recentDays" ? root.draft.rangeCountText || "" : ""
                            errorText: root.errorFor("dataRange", "", "recentDays")
                            onEdited: text => root.edit("dataRange", "", "recentDays", text)
                        }

                        GraphNumberField {
                            Layout.fillWidth: true
                            visible: root.draft.rangeKind === "latestRuns"
                            fieldName: "graphField_dataRange_latestRuns"
                            graphKey: root.graphKey
                            label: qsTr("Number of runs")
                            unit: qsTr("runs")
                            hint: qsTr("Whole runs, 1 to %1. The newest runs of the scenario are shown.")
                                .arg(root.num(root.limits.maxLatestRuns))
                            shown: root.draft.rangeKind === "latestRuns" ? root.draft.rangeCountText || "" : ""
                            errorText: root.errorFor("dataRange", "", "latestRuns")
                            onEdited: text => root.edit("dataRange", "", "latestRuns", text)
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: root.draft.rangeKind === "recentDays" || root.draft.rangeKind === "latestRuns"

                            Repeater {
                                model: root.draft.rangeKind === "latestRuns" ? GraphCatalog.latestRunPresets
                                                                             : GraphCatalog.recentDayPresets

                                Button {
                                    required property int modelData
                                    objectName: "graphRangePreset_" + root.draft.rangeKind + "_" + modelData
                                    text: root.draft.rangeKind === "latestRuns"
                                          ? qsTr("%1 runs").arg(root.num(modelData)) : root.daysText(modelData)
                                    highlighted: root.draft.rangeCount === modelData
                                    font.bold: highlighted
                                    Accessible.description: highlighted ? qsTr("Selected") : ""
                                    onClicked: root.edit("dataRange", "", root.draft.rangeKind, root.num(modelData))
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            visible: root.draft.rangeKind === "customDates"
                            columns: root.wide ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 10

                            GraphNumberField {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                fieldName: "graphField_dataRange_startDate"
                                graphKey: root.graphKey
                                label: qsTr("From (inclusive)")
                                placeholder: root.limits.dateFormat || ""
                                hint: qsTr("%1, from %2 to %3.").arg(root.limits.dateFormat).arg(root.limits.earliestDateText)
                                    .arg(root.limits.latestDateText)
                                shown: root.draft.rangeStartText || ""
                                errorText: root.errorFor("dataRange", "", "startDate")
                                onEdited: text => root.edit("dataRange", "", "startDate", text)
                            }
                            GraphNumberField {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                fieldName: "graphField_dataRange_endDate"
                                graphKey: root.graphKey
                                label: qsTr("To (inclusive)")
                                placeholder: root.limits.dateFormat || ""
                                shown: root.draft.rangeEndText || ""
                                errorText: root.errorFor("dataRange", "", "endDate")
                                onEdited: text => root.edit("dataRange", "", "endDate", text)
                            }
                        }

                        Label {
                            objectName: "graphRangeSummary"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.italic: true
                            text: {
                                const d = root.draft
                                switch (d.rangeKind) {
                                case "recentDays":
                                    return d.anchor
                                        ? qsTr("Moving range: the last %1, ending at your latest recorded run.").arg(root.daysText(d.rangeCount))
                                        : qsTr("Moving range: the last %1, ending today. It follows the calendar.").arg(root.daysText(d.rangeCount))
                                case "latestRuns":
                                    return qsTr("Moving range: your newest %1 runs. It follows new runs.").arg(root.num(d.rangeCount))
                                case "customDates":
                                    return qsTr("Fixed dates: %1 – %2, both days inclusive. They do not move.")
                                        .arg(d.rangeStartFormatted).arg(d.rangeEndFormatted)
                                }
                                return root.offersLatestRuns ? qsTr("Shows every recorded run.") : qsTr("Shows all recorded history.")
                            }
                        }

                        CheckBox {
                            id: anchorBox
                            objectName: "graphField_dataRange_anchor"
                            Layout.fillWidth: true
                            text: qsTr("End date ranges at latest recorded run")
                            checked: root.draft.anchor === true
                            Accessible.name: text
                            Accessible.description: qsTr("Applies to recent-day ranges. Custom dates stay as entered.")
                            onToggled: {
                                root.settingsVm.setGraphAnchor(root.graphKey, anchorBox.checked)
                                anchorBox.checked = Qt.binding(() => root.draft.anchor === true)
                            }
                        }
                    }
                }

                // ---- Lines & style ------------------------------------------------------------------------------
                GraphSettingsGroup {
                    id: seriesStyleGroup
                    groupName: "seriesStyle"
                    title: qsTr("Lines & style")
                    headerMinimumHeight: root.sections[0] === groupName ? resetButton.implicitHeight : 0

                    GraphLineStyleControls {
                        Layout.fillWidth: true
                        spacing: 10
                        compact: true
                        connection: root.draft.connection || ""
                        markers: root.draft.markers === true
                        onConnectionChosen: connection => root.settingsVm.setGraphConnection(root.graphKey, connection)
                        onMarkersChosen: markers => root.settingsVm.setGraphMarkers(root.graphKey, markers)
                    }

                    // Loaded only for the run graph: these rows rebuild on every edit, even while hidden.
                    Loader {
                        Layout.fillWidth: true
                        visible: root.isRunGraph
                        active: root.isRunGraph
                        sourceComponent: SeriesConfigDraftPanel {
                            settingsVm: root.settingsVm
                            scrollView: editorScroll
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.seriesRows.length > 0

                        Label {
                            Layout.fillWidth: true
                            text: root.seriesRows.length === 1 ? qsTr("Series") : qsTr("Series colours and widths")
                            font.bold: true
                        }
                        Label {
                            objectName: "graphSeriesWidthHint"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.75
                            text: qsTr("Line width in pixels, %1 to %2.").arg(root.decimal(root.limits.minWidth))
                                .arg(root.decimal(root.limits.maxWidth))
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Item { Layout.preferredWidth: 40 }
                            Label { objectName: "graphSeriesHeader_name"; Layout.fillWidth: true; text: qsTr("Name"); Accessible.ignored: true }
                            Label { objectName: "graphSeriesHeader_width"; Layout.preferredWidth: 64; text: qsTr("Width"); Accessible.ignored: true }
                            Item { Layout.preferredWidth: 20 }
                            Item { Layout.preferredWidth: 75 }
                        }
                        Repeater {
                            model: root.keyList(root.seriesKeys)

                            ColumnLayout {
                                id: row
                                required property string modelData
                                readonly property var entry: root.rowFor(root.seriesRows, modelData)
                                readonly property string seriesLabel: GraphCatalog.seriesName(root.graphKey, modelData)

                                objectName: "graphSeriesRow_" + modelData
                                Layout.fillWidth: true
                                spacing: 2

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: Math.max(36, rowContents.implicitHeight + 8)
                                    radius: 6
                                    color: swatch.palette.base
                                    RowLayout {
                                        id: rowContents
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        spacing: 8
                                        Button {
                                            id: swatch
                                            objectName: "graphField_seriesStyle_" + row.modelData + "_color"
                                            readonly property bool usesThemeColor: !row.entry.color
                                            readonly property color swatchColor: usesThemeColor ? palette.text : row.entry.color
                                            implicitWidth: 40
                                            implicitHeight: 26
                                            Accessible.name: qsTr("%1 colour").arg(row.seriesLabel)
                                            Accessible.description: usesThemeColor ? qsTr("Follows the theme until a colour is chosen") : ""
                                            onClicked: root.pickColor(row.entry)
                                            background: Rectangle {
                                                radius: 4
                                                color: swatch.swatchColor
                                                border.width: swatch.visualFocus ? 2 : 1
                                                border.color: swatch.visualFocus ? swatch.palette.highlight : swatch.palette.mid
                                            }
                                        }
                                        Label {
                                            objectName: "graphSeriesName_" + row.modelData
                                            Layout.fillWidth: true
                                            text: row.seriesLabel
                                            elide: Text.ElideRight
                                        }
                                        GraphTextInput {
                                            objectName: "graphField_seriesStyle_" + row.modelData + "_width"
                                            Layout.preferredWidth: 64
                                            graphKey: root.graphKey
                                            shown: row.entry.widthText || ""
                                            selectByMouse: true
                                            Accessible.name: qsTr("%1 line width").arg(row.seriesLabel)
                                            Accessible.description: root.errorFor("seriesStyle", row.modelData, "width")
                                            onEdited: text => root.edit("seriesStyle", row.modelData, "width", text)
                                        }
                                        Label { text: qsTr("px") }
                                        Button {
                                            objectName: "graphSeriesReset_" + row.modelData
                                            text: qsTr("Default")
                                            flat: true
                                            enabled: row.entry.colorOverridden === true || row.entry.widthOverridden === true
                                                     || root.errorFor("seriesStyle", row.modelData, "width") !== ""
                                            Accessible.name: qsTr("Restore default colour and width of %1").arg(row.seriesLabel)
                                            onClicked: {
                                                root.settingsVm.clearSeriesOverride(root.graphKey, row.modelData, "color")
                                                root.settingsVm.clearSeriesOverride(root.graphKey, row.modelData, "width")
                                            }
                                        }
                                    }
                                }
                                Label {
                                    objectName: "graphField_seriesStyle_" + row.modelData + "_width_error"
                                    Layout.fillWidth: true
                                    visible: text !== ""
                                    text: root.errorFor("seriesStyle", row.modelData, "width")
                                    color: Theme.errorText
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                // ---- Axes ---------------------------------------------------------------------------------------
                GraphSettingsGroup {
                    id: axesGroup
                    groupName: "axes"
                    title: qsTr("Axes")
                    visible: root.sections.includes("axes")

                    Loader {
                        Layout.fillWidth: true
                        visible: root.isRunGraph
                        active: root.isRunGraph
                        sourceComponent: AxesPanel {
                            settingsVm: root.settingsVm
                        }
                    }
                    Repeater {
                        model: root.keyList(root.axisKeys)

                        ColumnLayout {
                            id: axis
                            required property string modelData
                            readonly property var row: root.rowFor(root.axisRows, modelData)
                            readonly property string axisUnit: GraphCatalog.axisUnit(root.graphKey, modelData)
                            readonly property string axisTitle: qsTr("%1 axis").arg(GraphCatalog.axisName(root.graphKey, modelData))

                            objectName: "graphAxis_" + modelData
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: axis.axisTitle
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                objectName: "graphAxisSeries_" + axis.modelData
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                opacity: 0.75
                                text: qsTr("Applies to: %1").arg(
                                    (axis.row.seriesKeys || []).map(k => GraphCatalog.seriesName(root.graphKey, k)).join(", "))
                            }
                            Label { text: qsTr("Vertical fit") }
                            ComboBox {
                                id: fitCombo
                                objectName: "graphField_axes_" + axis.modelData + "_fitMode"
                                implicitContentWidthPolicy: ComboBox.WidestText
                                Layout.fillWidth: true
                                Layout.maximumWidth: Math.max(implicitWidth, fitMetrics.height * 16)
                                FontMetrics { id: fitMetrics; font: fitCombo.font }
                                readonly property var entries: root.fitEntries
                                model: entries
                                textRole: "text"
                                currentIndex: Math.max(0, entries.findIndex(e => e.mode === axis.row.mode))
                                Accessible.name: qsTr("%1 vertical fit").arg(axis.axisTitle)
                                Accessible.description: root.errorFor("axes", axis.modelData, "fitMode")
                                onActivated: index => {
                                    root.settingsVm.setAxisMode(root.graphKey, axis.modelData, entries[index].mode)
                                    fitCombo.currentIndex = Qt.binding(
                                        () => Math.max(0, entries.findIndex(e => e.mode === axis.row.mode)))
                                }
                            }
                            Label {
                                objectName: fitCombo.objectName + "_error"
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: root.errorFor("axes", axis.modelData, "fitMode")
                                color: Theme.errorText
                                wrapMode: Text.WordWrap
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                visible: axis.row.mode === "fixed"
                                columns: root.wide ? 2 : 1
                                columnSpacing: 16
                                rowSpacing: 10

                                GraphNumberField {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    fieldName: "graphField_axes_" + axis.modelData + "_fixedMin"
                                    graphKey: root.graphKey
                                    label: qsTr("Minimum")
                                    unit: axis.axisUnit
                                    hint: qsTr("Up to %1 in size.").arg(root.num(root.limits.maxBound))
                                    shown: axis.row.minText || ""
                                    errorText: root.errorFor("axes", axis.modelData, "fixedMin")
                                    onEdited: text => root.edit("axes", axis.modelData, "fixedMin", text)
                                }
                                GraphNumberField {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    fieldName: "graphField_axes_" + axis.modelData + "_fixedMax"
                                    graphKey: root.graphKey
                                    label: qsTr("Maximum")
                                    unit: axis.axisUnit
                                    shown: axis.row.maxText || ""
                                    errorText: root.errorFor("axes", axis.modelData, "fixedMax")
                                    onEdited: text => root.edit("axes", axis.modelData, "fixedMax", text)
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
            }
        }
    }
}
