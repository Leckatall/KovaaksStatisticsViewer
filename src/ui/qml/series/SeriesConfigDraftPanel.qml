import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import KovaaksStatsViewer

Pane {
    id: root

    required property var settingsVm
    property var sourceRows: settingsVm.allSeriesConfigs
    property var displayRows: []
    property var colorTarget: null
    property var axisNameTarget: null
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
    readonly property alias newAxisDialog: newAxisDialog
    readonly property alias newAxisNameField: newAxisNameField
    readonly property alias optionsPopup: optionsPopup
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
            text: qsTr("+ New axis..."),
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
            root.axisNameTarget = row
            newAxisDialog.open()
            return
        }
        root.settingsVm.updateSeriesAxis(row.id, value)
    }
    function openSeriesOptions(row, anchor) {
        const corner = anchor.mapToItem(root, anchor.width, anchor.height)
        optionsAnchorRight = corner.x
        optionsAnchorBottom = corner.y
        optionsTargetId = row.id
        optionsPopup.open()
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
    onOptionsTargetChanged: if (!optionsTarget) optionsPopup.close()

    ColorDialog {
        id: colorDialog
        options: ColorDialog.ShowAlphaChannel
        onAccepted: if (root.colorTarget) root.settingsVm.updateComputedSeries(root.colorTarget.id, root.colorTarget.name, selectedColor, root.colorTarget.width, root.colorTarget.enabled, root.colorTarget.expression)
    }
    ExpressionEditorDialog { id: expressionDialog; settingsVm: root.settingsVm }
    Dialog {
        id: newAxisDialog
        objectName: "newAxisDialog"
        title: qsTr("New axis")
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: newAxisNameField
            objectName: "newAxisNameField"
            placeholderText: qsTr("Axis name")
        }

        onAccepted: {
            const result = root.settingsVm.createAxis(newAxisNameField.text)
            if (result && result.succeeded && root.axisNameTarget)
                root.settingsVm.updateSeriesAxis(root.axisNameTarget.id, result.createdAxisId)
            newAxisNameField.text = ""
        }
    }

    Popup {
        id: optionsPopup
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
                    value: optionsPopup.hiddenLeadingPoints
                    onValueModified: root.settingsVm.setSeriesHiddenLeadingPoints(root.optionsTargetId, value)
                }
            }

            Label {
                objectName: "hiddenLeadingPointsHint"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 220
                text: optionsPopup.hiddenLeadingPoints === 0 ? qsTr("The whole line is drawn.")
                      : optionsPopup.hiddenLeadingPoints === 1 ? qsTr("The line starts after its first point.")
                      : qsTr("The line starts after its first %n points.", "", optionsPopup.hiddenLeadingPoints)
                color: optionsPopup.palette.placeholderText
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
                    optionsPopup.close()
                    root.settingsVm.removeComputedSeries(id)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            id: seriesListScrollView
            objectName: "seriesListScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            property int autoScrollDirection: 0
            property real autoScrollAccumulatedDelta: 0
            readonly property real autoScrollEdgeThreshold: 32
            readonly property real autoScrollSpeed: 6

            function applyAutoScrollTick() {
                const flick = contentItem
                const maxContentY = Math.max(0, flick.contentHeight - flick.height)
                const requestedContentY = flick.contentY + autoScrollDirection * autoScrollSpeed
                const clampedContentY = Math.max(0, Math.min(maxContentY, requestedContentY))
                const appliedDelta = clampedContentY - flick.contentY
                flick.contentY = clampedContentY
                if (appliedDelta === 0)
                    return

                autoScrollAccumulatedDelta += appliedDelta
                root.dragTranslationY = root.dragRawTranslationY + autoScrollAccumulatedDelta
                root.previewMove(root.dragOriginIndex + Math.round(root.dragTranslationY / root.dragRowHeight))
            }

            Timer {
                interval: 16
                running: seriesListScrollView.autoScrollDirection !== 0
                repeat: true
                onTriggered: seriesListScrollView.applyAutoScrollTick()
            }

            ColumnLayout {
                width: seriesListScrollView.availableWidth
                spacing: 5

                Repeater {
                    model: root.displayRows

                    SeriesConfigEditorDelegate {
                        displayRows: root.displayRows
                        dragState: root
                        scrollView: seriesListScrollView
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
        Label {
            id: seriesDraftErrorLabel
            Layout.fillWidth: true
            color: Theme.error
            objectName: "seriesDraftErrorLabel"
            text: (root.settingsVm.issues || []).length > 0
                  ? qsTr("Changes weren't saved because some settings are invalid. Your edits are still here; correct them and save again.")
                  : ""
            visible: text.length > 0
            wrapMode: Text.WordWrap
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
