import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var modelData
    property var row: modelData
    required property var displayRows
    required property var dragState
    required property var scrollView
    required property var axisComboModel
    property bool hovered: hoverHandler.hovered
    readonly property color restingColor: palette.base
    readonly property color hoverColor: Qt.lighter(restingColor, 1.12)
    readonly property color dragHandleDotColor: palette.light
    property real previewOffset: {
        if (!dragState.draggedSeriesId || dragState.dragOriginIndex < 0 || dragState.dragPreviewIndex < 0)
            return 0
        if (row.id === dragState.draggedSeriesId)
            return dragState.dragTranslationY

        const index = displayRows.findIndex(function(series) { return series.id === row.id })
        if (dragState.dragOriginIndex < dragState.dragPreviewIndex && index > dragState.dragOriginIndex && index <= dragState.dragPreviewIndex)
            return -dragState.dragRowHeight
        if (dragState.dragPreviewIndex < dragState.dragOriginIndex && index >= dragState.dragPreviewIndex && index < dragState.dragOriginIndex)
            return dragState.dragRowHeight
        return 0
    }

    signal colorRequested(var row)
    signal nameUpdateRequested(var row, string name)
    signal expressionEditRequested(var row)
    signal axisSelectionRequested(var row, string value)
    signal seriesRemovalRequested(var row)
    signal seriesEnabledRequested(var row, bool enabled)
    signal reorderRequested(string seriesId, int targetPosition)

    Layout.fillWidth: true
    implicitWidth: content.implicitWidth + 12
    implicitHeight: content.implicitHeight + 8
    color: hovered ? hoverColor : restingColor
    radius: 6
    z: dragState.draggedSeriesId === row.id ? 1 : 0
    transform: Translate { y: root.previewOffset }

    function axisComboIndexFor(yAxisId) {
        const id = yAxisId || ""
        return axisComboModel.findIndex(function(item) { return item.value === id })
    }

    HoverHandler {
        id: hoverHandler
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 4
        spacing: 5

    Item {
        id: dragHandle
        objectName: "seriesDragHandle_" + root.row.id
        Layout.preferredWidth: 12
        Layout.preferredHeight: 24

        Repeater {
            model: 6

            Rectangle {
                objectName: "seriesDragHandleDot_" + root.row.id + "_" + index
                width: 3
                height: 3
                x: 1.5 + (index % 2) * 6
                y: 4.5 + Math.floor(index / 2) * 6
                color: root.dragHandleDotColor
                radius: width / 2
                Accessible.ignored: true
            }
        }

        DragHandler {
            id: drag
            target: null
            dragThreshold: 0
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            cursorShape: Qt.PointingHandCursor

            onActiveChanged: {
                if (active) {
                    root.dragState.draggedSeriesId = root.row.id
                    root.dragState.dragOriginIndex = root.displayRows.findIndex(function(series) { return series.id === root.row.id })
                    root.dragState.dragPreviewIndex = root.dragState.dragOriginIndex
                    root.dragState.dragRowHeight = Math.max(root.height, 1)
                    root.dragState.dragRawTranslationY = 0
                    root.dragState.dragStartPointerY = dragHandle.mapToItem(root.scrollView, 0, 0).y
                    root.scrollView.autoScrollAccumulatedDelta = 0
                    root.scrollView.autoScrollDirection = 0
                } else if (root.dragState.draggedSeriesId === root.row.id) {
                    const seriesId = root.row.id
                    const targetPosition = root.dragState.dragPreviewIndex
                    root.dragState.draggedSeriesId = ""
                    root.dragState.dragOriginIndex = -1
                    root.dragState.dragPreviewIndex = -1
                    root.dragState.dragTranslationY = 0
                    root.dragState.dragRawTranslationY = 0
                    root.dragState.dragStartPointerY = 0
                    root.dragState.dragRowHeight = 0
                    root.scrollView.autoScrollAccumulatedDelta = 0
                    root.scrollView.autoScrollDirection = 0
                    // Reordering can synchronously reset the Repeater and destroy this delegate.
                    root.reorderRequested(seriesId, targetPosition)
                }
            }

            onTranslationChanged: {
                root.dragState.dragRawTranslationY = translation.y
                root.dragState.dragTranslationY = translation.y + root.scrollView.autoScrollAccumulatedDelta
                root.dragState.previewMove(root.dragState.dragOriginIndex + Math.round(root.dragState.dragTranslationY / root.dragState.dragRowHeight))

                const pointerY = root.dragState.dragStartPointerY + translation.y
                if (pointerY < root.scrollView.autoScrollEdgeThreshold)
                    root.scrollView.autoScrollDirection = -1
                else if (pointerY > root.scrollView.height - root.scrollView.autoScrollEdgeThreshold)
                    root.scrollView.autoScrollDirection = 1
                else
                    root.scrollView.autoScrollDirection = 0
            }
        }
    }

    Rectangle {
        objectName: "seriesColorSwatch_" + root.row.id
        Layout.preferredWidth: 16
        Layout.preferredHeight: 16
        border.color: root.palette.mid
        border.width: 1
        color: root.row.color
        radius: 4

        MouseArea {
            anchors.fill: parent
            onClicked: root.colorRequested(root.row)
        }
    }

    TextField {
        id: nameField
        objectName: "seriesNameField_" + root.row.id
        Layout.fillWidth: true
        text: root.row.name
        onEditingFinished: root.nameUpdateRequested(root.row, text)
        Component.onCompleted: cursorPosition = 0
    }

    Button {
        objectName: "editExpressionButton_" + root.row.id
        text: qsTr("ƒx")

        Layout.preferredWidth: contentItem.implicitWidth + (padding * 2)
        Accessible.name: qsTr("Edit Expression")
        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: qsTr("Edit Expression")
        onClicked: root.expressionEditRequested(root.row)
    }

    ComboBox {
        id: axisCombo
        objectName: "axisCombo_" + root.row.id
        model: root.axisComboModel
        textRole: "text"
        valueRole: "value"
        implicitContentWidthPolicy: ComboBox.WidestText
        Layout.minimumWidth: implicitContentWidth + (padding * 2) + 10
        Layout.preferredWidth: implicitContentWidth + (padding * 2) + 20
        currentIndex: root.axisComboIndexFor(root.row.yAxisId)
        onActivated: root.axisSelectionRequested(root.row, root.axisComboModel[currentIndex].value)
    }

    Button {
        objectName: "deleteSeriesButton_" + root.row.id
        text: qsTr("Delete")
        Layout.preferredWidth: contentItem.implicitWidth + (padding * 2)
        onClicked: root.seriesRemovalRequested(root.row)
    }

    Switch {
        checked: root.row.enabled
        objectName: "seriesEnabledSwitch_" + root.row.id
        onToggled: root.seriesEnabledRequested(root.row, checked)
    }
    }
}
