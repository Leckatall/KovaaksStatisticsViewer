import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "expressionEditorDialog"
    modal: true
    title: qsTr("Edit expression")
    width: 600
    height: 400
    standardButtons: Dialog.Save | Dialog.Cancel

    required property var settingsVm
    property string seriesId: ""
    property string seriesName: ""
    property color seriesColor: "transparent"
    property real seriesWidth: 2.0
    property bool seriesEnabled: true
    property var editorModel: null

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
