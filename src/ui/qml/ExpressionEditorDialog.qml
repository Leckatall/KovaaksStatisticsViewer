import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "expressionEditorDialog"
    modal: true
    title: qsTr("Edit expression")
    standardButtons: Dialog.Save | Dialog.Cancel

    required property var settingsVm
    property string seriesId: ""
    property string seriesName: ""
    property color seriesColor: "transparent"
    property real seriesWidth: 2.0
    property bool seriesEnabled: true
    property var editorModel: null

    // Popups are clipped by this surface, which can differ from the window's size.
    readonly property int overlayMargin: 40
    width: Overlay.overlay ? Math.min(implicitWidth, Math.max(0, Overlay.overlay.width - overlayMargin)) : implicitWidth
    height: Overlay.overlay ? Math.min(implicitHeight, Math.max(0, Overlay.overlay.height - overlayMargin)) : implicitHeight

    function beginEditing() {
        editorModel = settingsVm.beginExpressionEdit(seriesId)
    }
    onVisibleChanged: if (visible) beginEditing()
    onAccepted: settingsVm.updateComputedSeries(seriesId, seriesName, seriesColor, seriesWidth, seriesEnabled, editorModel.toExpressionMap())

    contentItem: Loader {
        id: treeEditorLoader
        active: root.editorModel !== null
        sourceComponent: Component {
            ExpressionTreeEditor {
                objectName: "expressionEditorTreeEditor"
                model: root.editorModel
            }
        }
    }
}
