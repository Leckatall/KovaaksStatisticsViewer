import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: root

    required property var settingsVm
    property var sourceRows: settingsVm.allSeriesConfigs
    property var displayRows: []
    property var colorTarget: null
    property var axisNameTarget: null
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
                        onSeriesRemovalRequested: row => root.settingsVm.removeComputedSeries(row.id)
                        onSeriesEnabledRequested: (row, enabled) => root.settingsVm.setSeriesEnabled(row.id, enabled)
                        onReorderRequested: (seriesId, targetPosition) => root.settingsVm.reorderSeries(seriesId, targetPosition)
                    }
                }
            }
        }
        Button {
            objectName: "addSeriesButton"
            text: qsTr("Add series")
            onClicked: root.settingsVm.createComputedSeries(qsTr("New Series"), "#4CAF50", 2.0, true, {})
        }
        Label {
            id: seriesDraftErrorLabel
            Layout.fillWidth: true
            color: "#E53935"
            objectName: "seriesDraftErrorLabel"
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                enabled: root.settingsVm.pendingChanges
                objectName: "saveGraphLinesButton"
                text: qsTr("Save Graph Lines")
                onClicked: {
                    const result = root.settingsVm.commitSeriesDraft()
                    seriesDraftErrorLabel.text = result && result.succeeded === false ? qsTr("Couldn't save graph line changes. Your edits are still here — try again.") : ""
                }
            }
            Button {
                enabled: root.settingsVm.pendingChanges
                objectName: "discardGraphLineChangesButton"
                text: qsTr("Discard Graph Line Changes")
                onClicked: root.settingsVm.discardSeriesDraft()
            }
        }
    }
}
