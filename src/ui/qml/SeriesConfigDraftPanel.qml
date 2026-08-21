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
    property string draggedSeriesId: ""
    property int dragOriginIndex: -1
    property int dragPreviewIndex: -1
    property real dragTranslationY: 0
    property real dragRawTranslationY: 0
    property real dragStartPointerY: 0
    property real dragRowHeight: 0
    readonly property alias colorDialog: colorDialog
    readonly property alias expressionDialog: expressionDialog

    objectName: "seriesConfigDraftPanel"
    spacing: 8

    function refreshRows() {
        displayRows = sourceRows.slice().sort(function(a, b) { return a.displayPosition - b.displayPosition })
    }
    function updateSeries(row, name, color) {
        settingsVm.updateComputedSeries(row.id, name, color, row.width, row.enabled, row.expression)
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
        onAccepted: if (root.colorTarget) root.updateSeries(root.colorTarget, root.colorTarget.name, selectedColor)
    }
    ExpressionEditorDialog { id: expressionDialog; settingsVm: root.settingsVm }

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
                spacing: 8

                Repeater {
                    model: root.displayRows

                    RowLayout {
                        id: lineRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        z: root.draggedSeriesId === modelData.id ? 1 : 0
                        transform: Translate { y: root.previewOffset(lineRow.modelData.id) }

                        Rectangle {
                            id: dragHandle
                            objectName: "seriesDragHandle_" + lineRow.modelData.id
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 24
                            color: root.palette.mid
                            radius: 2
                            DragHandler {
                                id: drag
                                target: null
                                dragThreshold: 0
                                grabPermissions: PointerHandler.CanTakeOverFromAnything
                                onActiveChanged: {
                                    if (active) {
                                        root.draggedSeriesId = lineRow.modelData.id
                                        root.dragOriginIndex = root.displayRows.findIndex(function(row) { return row.id === lineRow.modelData.id })
                                        root.dragPreviewIndex = root.dragOriginIndex
                                        root.dragRowHeight = Math.max(lineRow.height, 1)
                                        root.dragRawTranslationY = 0
                                        root.dragStartPointerY = dragHandle.mapToItem(seriesListScrollView, 0, 0).y
                                        seriesListScrollView.autoScrollAccumulatedDelta = 0
                                        seriesListScrollView.autoScrollDirection = 0
                                    } else if (root.draggedSeriesId === lineRow.modelData.id) {
                                        const seriesId = lineRow.modelData.id
                                        const targetPosition = root.dragPreviewIndex
                                        root.draggedSeriesId = ""
                                        root.dragOriginIndex = -1
                                        root.dragPreviewIndex = -1
                                        root.dragTranslationY = 0
                                        root.dragRawTranslationY = 0
                                        root.dragStartPointerY = 0
                                        root.dragRowHeight = 0
                                        seriesListScrollView.autoScrollAccumulatedDelta = 0
                                        seriesListScrollView.autoScrollDirection = 0
                                        // reorderSeries() can synchronously reset the Repeater (allSeriesConfigs ->
                                        // sourceRows -> displayRows), destroying this delegate mid-call; nothing above
                                        // this line touches root or lineRow again, and nothing may be added after it.
                                        root.settingsVm.reorderSeries(seriesId, targetPosition)
                                    }
                                }
                                onTranslationChanged: {
                                    root.dragRawTranslationY = translation.y
                                    root.dragTranslationY = translation.y + seriesListScrollView.autoScrollAccumulatedDelta
                                    root.previewMove(root.dragOriginIndex + Math.round(root.dragTranslationY / root.dragRowHeight))

                                    const pointerY = root.dragStartPointerY + translation.y
                                    if (pointerY < seriesListScrollView.autoScrollEdgeThreshold)
                                        seriesListScrollView.autoScrollDirection = -1
                                    else if (pointerY > seriesListScrollView.height - seriesListScrollView.autoScrollEdgeThreshold)
                                        seriesListScrollView.autoScrollDirection = 1
                                    else
                                        seriesListScrollView.autoScrollDirection = 0
                                }
                            }
                        }
                        Rectangle {
                            objectName: "seriesColorSwatch_" + lineRow.modelData.id
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            border.color: root.palette.mid
                            border.width: 1
                            color: lineRow.modelData.color
                            radius: 4
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.colorTarget = lineRow.modelData
                                    colorDialog.selectedColor = lineRow.modelData.color
                                    colorDialog.open()
                                }
                            }
                        }
                        TextField {
                            id: nameField
                            objectName: "seriesNameField_" + lineRow.modelData.id
                            Layout.fillWidth: true
                            text: lineRow.modelData.name
                            onEditingFinished: root.updateSeries(lineRow.modelData, text, lineRow.modelData.color)
                        }
                        Button {
                            objectName: "editExpressionButton_" + lineRow.modelData.id
                            text: qsTr("Edit expression")
                            onClicked: {
                                expressionDialog.seriesId = lineRow.modelData.id
                                expressionDialog.seriesName = lineRow.modelData.name
                                expressionDialog.seriesColor = lineRow.modelData.color
                                expressionDialog.seriesWidth = lineRow.modelData.width
                                expressionDialog.seriesEnabled = lineRow.modelData.enabled
                                expressionDialog.open()
                            }
                        }
                        Button {
                            objectName: "deleteSeriesButton_" + lineRow.modelData.id
                            text: qsTr("Delete")
                            onClicked: root.settingsVm.removeComputedSeries(lineRow.modelData.id)
                        }
                        Switch {
                            checked: lineRow.modelData.enabled
                            objectName: "seriesEnabledSwitch_" + lineRow.modelData.id
                            onToggled: root.settingsVm.setSeriesEnabled(lineRow.modelData.id, checked)
                        }
                    }
                }
            }
        }
        Button {
            objectName: "addSeriesButton"
            text: qsTr("Add series")
            onClicked: root.settingsVm.createComputedSeries(qsTr("New Series"), "#4CAF50", 2.0, true, { kind: "primitive", primitiveMetric: "score" })
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
