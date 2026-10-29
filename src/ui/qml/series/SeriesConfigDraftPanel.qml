pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import KovaaksStatsViewer

Pane {
    id: root

    required property var settingsVm
    required property ScrollView scrollView
    property var sourceRows: settingsVm.allSeriesConfigs
    property var displayRows: []
    property var headerFirstRow: null
    property var colorTarget: null
    // The options popup lives here rather than in a row: every edit republishes allSeriesConfigs,
    // which rebuilds every row delegate and would destroy a popup owned by one.
    property string optionsTargetId: ""
    readonly property var optionsTarget: displayRows.find(function(row) { return row.id === optionsTargetId }) || null
    property real optionsAnchorRight: 0
    property real optionsAnchorBottom: 0
    property string draggedSeriesId: ""
    property int dragOriginIndex: -1
    property int dragPreviewIndex: -1
    property real dragTranslationY: 0
    property real dragRawTranslationY: 0
    property real dragStartPointerY: 0
    property real dragRowHeight: 0
    readonly property alias colorDialog: colorDialog
    readonly property alias expressionDialog: expressionDialog
    readonly property alias newAxisDialog: axisCreationDialog
    readonly property alias newAxisNameField: axisCreationDialog.nameField
    readonly property alias optionsPopup: seriesOptionsPopup
    readonly property var axisComboModel: {
        const items = [{
            text: qsTr("Independent"),
            value: ""
        }]
        for (const axis of settingsVm.allAxes)
            items.push({
                text: axis.name,
                value: axis.id
            })
        items.push({
            text: qsTr("+ New axis…"),
            value: "__new__"
        })
        return items
    }

    objectName: "seriesConfigDraftPanel"
    spacing: 8

    function refreshRows() {
        displayRows = sourceRows.slice().sort(function(a, b) { return a.displayPosition - b.displayPosition })
    }
    function handleAxisSelection(row, value) {
        if (value === "__new__") {
            axisCreationDialog.openFor(row.id)
            return
        }
        root.settingsVm.updateSeriesAxis(row.id, value)
    }
    function openSeriesOptions(row, anchor) {
        const corner = anchor.mapToItem(root, anchor.width, anchor.height)
        optionsAnchorRight = corner.x
        optionsAnchorBottom = corner.y
        optionsTargetId = row.id
        seriesOptionsPopup.open()
    }
    function previewMove(position) {
        dragPreviewIndex = Math.max(0, Math.min(displayRows.length - 1, position))
    }
    function previewOffset(id) {
        if (!draggedSeriesId || dragOriginIndex < 0 || dragPreviewIndex < 0)
            return 0
        if (id === draggedSeriesId)
            return dragTranslationY

        const index = displayRows.findIndex(function(row) { return row.id === id })
        if (dragOriginIndex < dragPreviewIndex && index > dragOriginIndex && index <= dragPreviewIndex)
            return -dragRowHeight
        if (dragPreviewIndex < dragOriginIndex && index >= dragPreviewIndex && index < dragOriginIndex)
            return dragRowHeight
        return 0
    }

    Component.onCompleted: refreshRows()
    onSourceRowsChanged: refreshRows()
    onOptionsTargetChanged: if (!optionsTarget) seriesOptionsPopup.close()

    ColorDialog {
        id: colorDialog
        options: ColorDialog.ShowAlphaChannel
        onAccepted: if (root.colorTarget) root.settingsVm.updateComputedSeries(root.colorTarget.id, root.colorTarget.name, selectedColor, root.colorTarget.width, root.colorTarget.enabled, root.colorTarget.expression)
    }
    ExpressionEditorDialog { id: expressionDialog; settingsVm: root.settingsVm }
    AxisNameDialog { id: axisCreationDialog; settingsVm: root.settingsVm }

    Popup {
        id: seriesOptionsPopup
        objectName: "seriesOptionsPopup"
        readonly property int hiddenLeadingPoints: root.optionsTarget ? root.optionsTarget.hiddenLeadingPoints || 0 : 0

        x: root.optionsAnchorRight - width
        y: root.optionsAnchorBottom + 4
        padding: 12
        margins: 8
        popupType: Popup.Item
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: root.optionsTargetId = ""

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                spacing: 10

                Label {
                    text: qsTr("Hide leading points")
                }

                SpinBox {
                    objectName: "hiddenLeadingPointsSpinBox_" + root.optionsTargetId
                    from: 0
                    to: root.settingsVm.maximumHiddenLeadingPoints
                    value: seriesOptionsPopup.hiddenLeadingPoints
                    onValueModified: root.settingsVm.setSeriesHiddenLeadingPoints(root.optionsTargetId, value)
                }
            }

            Label {
                objectName: "hiddenLeadingPointsHint"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 220
                text: seriesOptionsPopup.hiddenLeadingPoints === 0 ? qsTr("The whole line is drawn.")
                      : seriesOptionsPopup.hiddenLeadingPoints === 1 ? qsTr("The line starts after its first point.")
                      : qsTr("The line starts after its first %n points.", "", seriesOptionsPopup.hiddenLeadingPoints)
                color: seriesOptionsPopup.palette.placeholderText
                wrapMode: Text.WordWrap
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Button {
                objectName: "deleteSeriesButton_" + root.optionsTargetId
                Layout.fillWidth: true
                flat: true
                text: qsTr("Delete series")
                icon.source: "../icons/trash-2.svg"
                icon.width: 16
                icon.height: 16
                onClicked: {
                    const id = root.optionsTargetId
                    seriesOptionsPopup.close()
                    root.settingsVm.removeComputedSeries(id)
                }
            }
        }
    }

    Item {
        id: autoScroll
        parent: root.scrollView
        // Covers the viewport, which sits inside the ScrollView's padding.
        x: root.scrollView.leftPadding
        y: root.scrollView.topPadding
        width: root.scrollView.availableWidth
        height: root.scrollView.availableHeight
        property int autoScrollDirection: 0
        property real autoScrollAccumulatedDelta: 0
        readonly property real autoScrollEdgeThreshold: 32
        readonly property real autoScrollSpeed: 6
        function applyAutoScrollTick() {
            const flick = root.scrollView.contentItem as Flickable
            const maxContentY = Math.max(0, flick.contentHeight - flick.height)
            const next = Math.max(0, Math.min(maxContentY, flick.contentY + autoScrollDirection * autoScrollSpeed))
            const delta = next - flick.contentY
            flick.contentY = next
            if (delta === 0)
                return
            autoScrollAccumulatedDelta += delta
            root.dragTranslationY = root.dragRawTranslationY + autoScrollAccumulatedDelta
            root.previewMove(root.dragOriginIndex + Math.round(root.dragTranslationY / root.dragRowHeight))
        }
        Timer {
            interval: 16
            running: autoScroll.autoScrollDirection !== 0
            repeat: true
            onTriggered: autoScroll.applyAutoScrollTick()
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 5

        Item {
            id: columnHeaders
            Layout.fillWidth: true
            implicitHeight: 20
            readonly property var firstRow: root.headerFirstRow
            Repeater {
                model: [
                    { key: "name", text: qsTr("Name"), property: "nameControl" },
                    { key: "expression", text: qsTr("Expr."), property: "expressionControl" },
                    { key: "axis", text: qsTr("Axis"), property: "axisControl" },
                    { key: "more", text: qsTr("More"), property: "moreControl" },
                    { key: "visible", text: qsTr("Visible"), property: "visibleControl" }
                ]
                Label {
                    required property var modelData
                    readonly property var target: columnHeaders.firstRow ? columnHeaders.firstRow[modelData.property] : null
                    objectName: "seriesColumnHeader_" + modelData.key
                    text: modelData.text
                    Accessible.ignored: true
                    x: target ? target.x + target.parent.x + columnHeaders.firstRow.x + (target.width - width) / 2 : 0
                }
            }
        }
        Repeater {
            onItemAdded: (index, item) => { if (index === 0) root.headerFirstRow = item }
            onItemRemoved: (index, item) => { if (root.headerFirstRow === item) root.headerFirstRow = null }
            model: root.displayRows

            SeriesConfigEditorDelegate {
                displayRows: root.displayRows
                dragState: root
                scrollView: autoScroll
                axisComboModel: root.axisComboModel
                onColorRequested: row => {
                    root.colorTarget = row
                    colorDialog.selectedColor = row.color
                    colorDialog.open()
                }
                onNameUpdateRequested: (row, name) => root.settingsVm.updateComputedSeries(row.id, name, row.color, row.width, row.enabled, row.expression)
                onExpressionEditRequested: row => {
                    expressionDialog.seriesId = row.id
                    expressionDialog.seriesName = row.name
                    expressionDialog.seriesColor = row.color
                    expressionDialog.seriesWidth = row.width
                    expressionDialog.seriesEnabled = row.enabled
                    expressionDialog.open()
                }
                onAxisSelectionRequested: (row, value) => root.handleAxisSelection(row, value)
                onOptionsRequested: (row, anchor) => root.openSeriesOptions(row, anchor)
                onSeriesEnabledRequested: (row, enabled) => root.settingsVm.setSeriesEnabled(row.id, enabled)
                onReorderRequested: (seriesId, targetPosition) => root.settingsVm.reorderSeries(seriesId, targetPosition)
            }
        }

        RowLayout {
            Button {
                objectName: "addSeriesButton"
                text: qsTr("Add series")
                onClicked: root.settingsVm.createComputedSeries(qsTr("New Series"), "#4CAF50", 3.0, true, "")
            }
            Button {
                Accessible.name: qsTr("Restore the default graph lines")
                objectName: "restoreDefaultGraphLinesButton"
                text: qsTr("Restore default lines")
                onClicked: root.settingsVm.restoreDefaultGraphLines()
            }
        }
    }

    // A handler's cursorShape only applies over its own item, so this holds the closed hand once the pointer leaves the handle.
    Item {
        id: dragCursorOverlay
        parent: root
        objectName: "seriesDragCursorOverlay"
        anchors.fill: parent
        enabled: root.draggedSeriesId !== ""
        visible: enabled
        z: 100

        HoverHandler {
            cursorShape: Qt.ClosedHandCursor
        }
    }
}
