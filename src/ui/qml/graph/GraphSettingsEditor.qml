import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import KovaaksStatsViewer
import "GraphCatalog.js" as GraphCatalog

// The Graphs settings page's editor for one graph: Data & range, Axes and Series & style, each only when the
// graph has something to put in it. Every control edits the settings visit's working copy through
// settingsVm; numbers and dates go to the view model as typed and are read there, in the app locale.
Item {
    id: root
    objectName: "graphSettingsEditor"

    required property var settingsVm
    // One of run, dailyPlaytime, scenarioHistory, benchmarkRank, benchmarkPlaytime.
    required property string graphKey

    // The run graph hands its individual lines to Graph Lines.
    signal graphLinesRequested()
    signal issueActivated(var issue)

    readonly property alias colorDialog: colorDialog
    readonly property alias scroller: scroller

    readonly property var draft: (settingsVm.graphDrafts || {})[graphKey] || ({})
    readonly property var limits: settingsVm.graphLimits || ({})
    readonly property string graphName: GraphCatalog.nameOf(graphKey)
    readonly property bool offersRange: draft.hasRange === true
    readonly property bool offersLatestRuns: draft.hasLatestRuns === true
    readonly property bool offersWindow: draft.hasWindow === true
    readonly property var axisRows: draft.axes || []
    readonly property var seriesRows: draft.series || []
    readonly property bool isRunGraph: graphKey === "run"
    readonly property bool pointsOnly: draft.connection === "pointsOnly"
    readonly property bool wide: scroller.availableWidth >= 560

    readonly property var sortedIssues: GraphCatalog.sortedIssues(settingsVm.draftIssues || [])
    readonly property bool summaryShown: (settingsVm.issues || []).length > 0 && sortedIssues.length > 0

    readonly property var rangeKinds: {
        const kinds = [
            {kind: "allHistory", text: offersLatestRuns ? qsTr("All runs") : qsTr("All time")},
            {kind: "recentDays", text: qsTr("Recent days")}
        ]
        if (offersLatestRuns) kinds.push({kind: "latestRuns", text: qsTr("Latest runs")})
        kinds.push({kind: "customDates", text: qsTr("Custom dates")})
        return kinds
    }
    readonly property int rangeIndex: Math.max(0, rangeKinds.findIndex(k => k.kind === draft.rangeKind))

    readonly property var connectionEntries: [
        {text: qsTr("Smooth lines"), value: "smooth"},
        {text: qsTr("Straight lines"), value: "straight"},
        {text: qsTr("Points only"), value: "pointsOnly"}
    ]
    readonly property var fitEntries: [
        {text: qsTr("Fit visible data"), mode: "fit"},
        {text: qsTr("Start at zero"), mode: "zero"},
        {text: qsTr("Fixed bounds"), mode: "fixed"}
    ]

    function num(value) {
        return Number(value).toLocaleString(Qt.locale(), "f", 0)
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

    function issueFor(group, target, field) {
        return (root.settingsVm.draftIssues || []).find(
            i => i.graph === root.graphKey && i.group === group && i.target === target && i.field === field) || null
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

    function controlLabel(issue) {
        switch (issue.group) {
        case "dataRange":
            return ({
                range: qsTr("Visible range"), recentDays: qsTr("Number of days"), latestRuns: qsTr("Number of runs"),
                startDate: qsTr("Start date"), endDate: qsTr("End date"), averagingWindow: qsTr("Rolling average")
            })[issue.field] || qsTr("Visible range")
        case "axes": {
            const axis = qsTr("%1 axis").arg(GraphCatalog.axisName(issue.graph, issue.target))
            const part = ({fixedMin: qsTr("minimum"), fixedMax: qsTr("maximum")})[issue.field] || qsTr("fit")
            return axis + " " + part
        }
        case "seriesStyle": {
            const series = GraphCatalog.seriesName(issue.graph, issue.target)
            return issue.field === "width" ? qsTr("%1 line width").arg(series) : series
        }
        }
        return issue.target
    }

    function locationText(issue) {
        const graph = GraphCatalog.nameOf(issue.graph)
        const group = GraphCatalog.groupName(issue.group)
        return issue.group === "graphLines" ? graph + " › " + group : graph + " › " + group + " › " + controlLabel(issue)
    }

    // ---- Focus and scrolling ----------------------------------------------------------------------------

    function ensureVisible(item) {
        const flick = scroller.contentItem
        const top = item.mapToItem(flick.contentItem, 0, 0).y
        const bottom = top + item.height
        const margin = 8
        if (top - margin < flick.contentY)
            flick.contentY = Math.max(0, top - margin)
        else if (bottom + margin > flick.contentY + flick.height)
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, bottom + margin - flick.height))
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

    function contains(item) {
        for (let current = item; current; current = current.parent)
            if (current === content) return true
        return false
    }

    // Tabbing to a control that is scrolled out of view brings it into view.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            const focused = root.Window.window ? root.Window.window.activeFocusItem : null
            if (focused && root.contains(focused)) root.ensureVisible(focused)
        }
    }

    function pickColor(row) {
        colorDialog.seriesKey = row.key
        colorDialog.selectedColor = row.color ? row.color : scroller.palette.text
        colorDialog.open()
    }

    ColorDialog {
        id: colorDialog
        property string seriesKey
        options: ColorDialog.ShowAlphaChannel
        onAccepted: root.settingsVm.setSeriesColor(root.graphKey, seriesKey, selectedColor)
    }

    OverflowScrollView {
        id: scroller
        objectName: "graphEditorScroll"
        anchors.fill: parent

        ColumnLayout {
            id: content
            objectName: "graphSettingsContent"
            width: scroller.availableWidth
            spacing: 16

            // ---- Issues of the last rejected Save --------------------------------------------------------
            Frame {
                objectName: "graphIssueSummary"
                Layout.fillWidth: true
                visible: root.summaryShown

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Fix these before saving. Nothing was saved.")
                        font.bold: true
                        wrapMode: Text.WordWrap
                        color: Theme.errorText
                    }
                    Repeater {
                        model: root.summaryShown ? root.sortedIssues : []

                        Button {
                            id: link
                            required property var modelData
                            required property int index
                            objectName: "graphIssueLink_" + index
                            Layout.fillWidth: true
                            flat: true
                            text: root.locationText(modelData) + ": " + root.messageFor(modelData)
                            Accessible.name: text
                            onClicked: root.issueActivated(modelData)
                            contentItem: Label {
                                text: link.text
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Label {
                objectName: "graphEditingLabel"
                Layout.fillWidth: true
                text: qsTr("Changes apply to %1.").arg(root.graphName)
                wrapMode: Text.WordWrap
            }

            // ---- Data & range ------------------------------------------------------------------------------
            GraphSettingsGroup {
                groupName: "dataRange"
                title: qsTr("Data & range")
                visible: root.offersRange || root.offersWindow

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 2 : 1
                    columnSpacing: 16
                    rowSpacing: 10
                    visible: root.offersRange || root.offersWindow

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 2
                        visible: root.offersRange

                        Label { text: qsTr("Visible range") }
                        ComboBox {
                            id: rangeCombo
                            objectName: "graphField_dataRange_range"
                            Layout.fillWidth: true
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
                            model: root.draft.rangeKind === "latestRuns" ? [25, 50, 100] : [7, 30, 90]

                            Button {
                                required property int modelData
                                objectName: "graphRangePreset_" + root.draft.rangeKind + "_" + modelData
                                text: root.draft.rangeKind === "latestRuns"
                                      ? qsTr("%1 runs").arg(root.num(modelData)) : root.daysText(modelData)
                                highlighted: root.draft.rangeCount === modelData
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

            // ---- Axes ---------------------------------------------------------------------------------------
            GraphSettingsGroup {
                groupName: "axes"
                title: qsTr("Axes")
                visible: root.axisRows.length > 0

                Repeater {
                    model: root.axisRows

                    ColumnLayout {
                        id: axis
                        required property var modelData
                        readonly property string axisUnit: GraphCatalog.axisUnit(root.graphKey, modelData.key)
                        readonly property string axisTitle: qsTr("%1 axis").arg(GraphCatalog.axisName(root.graphKey, modelData.key))

                        objectName: "graphAxis_" + modelData.key
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: axis.axisTitle
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            objectName: "graphAxisSeries_" + axis.modelData.key
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.75
                            text: qsTr("Applies to: %1").arg(
                                axis.modelData.seriesKeys.map(k => GraphCatalog.seriesName(root.graphKey, k)).join(", "))
                        }
                        Label { text: qsTr("Vertical fit") }
                        ComboBox {
                            id: fitCombo
                            objectName: "graphField_axes_" + axis.modelData.key + "_fitMode"
                            Layout.fillWidth: true
                            readonly property var entries: root.fitEntries
                            model: entries
                            textRole: "text"
                            currentIndex: Math.max(0, entries.findIndex(e => e.mode === axis.modelData.mode))
                            Accessible.name: qsTr("%1 vertical fit").arg(axis.axisTitle)
                            Accessible.description: root.errorFor("axes", axis.modelData.key, "fitMode")
                            onActivated: index => {
                                root.settingsVm.setAxisMode(root.graphKey, axis.modelData.key, entries[index].mode)
                                fitCombo.currentIndex = Qt.binding(
                                    () => Math.max(0, entries.findIndex(e => e.mode === axis.modelData.mode)))
                            }
                        }
                        Label {
                            objectName: fitCombo.objectName + "_error"
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: root.errorFor("axes", axis.modelData.key, "fitMode")
                            color: Theme.errorText
                            wrapMode: Text.WordWrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            visible: axis.modelData.mode === "fixed"
                            columns: root.wide ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 10

                            GraphNumberField {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                fieldName: "graphField_axes_" + axis.modelData.key + "_fixedMin"
                                graphKey: root.graphKey
                                label: qsTr("Minimum")
                                unit: axis.axisUnit
                                hint: qsTr("Up to %1 in size.").arg(root.num(root.limits.maxBound))
                                shown: axis.modelData.minText || ""
                                errorText: root.errorFor("axes", axis.modelData.key, "fixedMin")
                                onEdited: text => root.edit("axes", axis.modelData.key, "fixedMin", text)
                            }
                            GraphNumberField {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                fieldName: "graphField_axes_" + axis.modelData.key + "_fixedMax"
                                graphKey: root.graphKey
                                label: qsTr("Maximum")
                                unit: axis.axisUnit
                                shown: axis.modelData.maxText || ""
                                errorText: root.errorFor("axes", axis.modelData.key, "fixedMax")
                                onEdited: text => root.edit("axes", axis.modelData.key, "fixedMax", text)
                            }
                        }
                    }
                }
            }

            // ---- Series & style -----------------------------------------------------------------------------
            GraphSettingsGroup {
                groupName: "seriesStyle"
                title: qsTr("Series & style")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label { text: qsTr("Line style") }
                    ComboBox {
                        id: connectionCombo
                        objectName: "graphField_seriesStyle_connection"
                        Layout.fillWidth: true
                        model: root.connectionEntries
                        textRole: "text"
                        currentIndex: Math.max(0, root.connectionEntries.findIndex(e => e.value === root.draft.connection))
                        Accessible.name: qsTr("Line style")
                        onActivated: index => {
                            root.settingsVm.setGraphConnection(root.graphKey, root.connectionEntries[index].value)
                            connectionCombo.currentIndex = Qt.binding(
                                () => Math.max(0, root.connectionEntries.findIndex(e => e.value === root.draft.connection)))
                        }
                    }
                }

                CheckBox {
                    id: markerBox
                    objectName: "graphField_seriesStyle_markers"
                    Layout.fillWidth: true
                    text: qsTr("Show point markers")
                    checked: root.pointsOnly || root.draft.markers === true
                    enabled: !root.pointsOnly
                    Accessible.name: text
                    Accessible.description: root.pointsOnly ? qsTr("Always shown while Points only is selected") : ""
                    onToggled: {
                        root.settingsVm.setGraphMarkers(root.graphKey, markerBox.checked)
                        markerBox.checked = Qt.binding(() => root.pointsOnly || root.draft.markers === true)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.isRunGraph

                    Label {
                        objectName: "graphLinesNote"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Individual lines are managed in Graph Lines: their colours, widths, axes and expressions.")
                    }
                    Button {
                        objectName: "graphLinesLink"
                        text: qsTr("Edit individual lines in Graph Lines…")
                        Accessible.name: text
                        onClicked: root.graphLinesRequested()
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

                    Repeater {
                        model: root.seriesRows

                        ColumnLayout {
                            id: row
                            required property var modelData
                            readonly property string seriesLabel: GraphCatalog.seriesName(root.graphKey, modelData.key)

                            objectName: "graphSeriesRow_" + modelData.key
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    objectName: "graphSeriesName_" + row.modelData.key
                                    Layout.fillWidth: true
                                    text: row.seriesLabel
                                    elide: Text.ElideRight
                                }
                                Button {
                                    id: swatch
                                    objectName: "graphField_seriesStyle_" + row.modelData.key + "_color"
                                    readonly property bool usesThemeColor: !row.modelData.color
                                    readonly property color swatchColor: usesThemeColor ? palette.text : row.modelData.color
                                    implicitWidth: 40
                                    implicitHeight: 26
                                    Accessible.name: qsTr("%1 colour").arg(row.seriesLabel)
                                    Accessible.description: usesThemeColor ? qsTr("Follows the theme until a colour is chosen") : ""
                                    onClicked: root.pickColor(row.modelData)
                                    background: Rectangle {
                                        radius: 4
                                        color: swatch.swatchColor
                                        border.width: swatch.visualFocus ? 2 : 1
                                        border.color: swatch.visualFocus ? swatch.palette.highlight : swatch.palette.mid
                                    }
                                }
                                GraphTextInput {
                                    objectName: "graphField_seriesStyle_" + row.modelData.key + "_width"
                                    Layout.preferredWidth: 64
                                    graphKey: root.graphKey
                                    shown: row.modelData.widthText
                                    selectByMouse: true
                                    Accessible.name: qsTr("%1 line width").arg(row.seriesLabel)
                                    Accessible.description: root.errorFor("seriesStyle", row.modelData.key, "width")
                                    onEdited: text => root.edit("seriesStyle", row.modelData.key, "width", text)
                                }
                                Label { text: qsTr("px") }
                                Button {
                                    objectName: "graphSeriesReset_" + row.modelData.key
                                    text: qsTr("Default")
                                    flat: true
                                    enabled: row.modelData.colorOverridden === true || row.modelData.widthOverridden === true
                                             || root.errorFor("seriesStyle", row.modelData.key, "width") !== ""
                                    Accessible.name: qsTr("Restore default colour and width of %1").arg(row.seriesLabel)
                                    onClicked: {
                                        root.settingsVm.clearSeriesOverride(root.graphKey, row.modelData.key, "color")
                                        root.settingsVm.clearSeriesOverride(root.graphKey, row.modelData.key, "width")
                                    }
                                }
                            }
                            Label {
                                objectName: "graphField_seriesStyle_" + row.modelData.key + "_width_error"
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: root.errorFor("seriesStyle", row.modelData.key, "width")
                                color: Theme.errorText
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // ---- Reset --------------------------------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Button {
                    objectName: "graphResetButton"
                    text: qsTr("Reset this graph")
                    Accessible.name: qsTr("Reset %1 to its defaults").arg(root.graphName)
                    onClicked: root.settingsVm.resetGraph(root.graphKey)
                }
                Label {
                    objectName: "graphResetNote"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.75
                    text: qsTr("Stages this graph's defaults for its range, averaging, line style, colours and axes; Save or Cancel decides. Graph Lines and other graphs are not changed.")
                }
            }

            Item { Layout.preferredHeight: 4 }
        }
    }
}
