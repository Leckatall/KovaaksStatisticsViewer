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
    implicitWidth: 400
    width: Overlay.overlay ? Math.min(implicitWidth, Math.max(0, Overlay.overlay.width - overlayMargin)) : implicitWidth
    height: Overlay.overlay ? Math.min(implicitHeight, Math.max(0, Overlay.overlay.height - overlayMargin)) : implicitHeight

    function beginEditing() {
        editorModel = settingsVm.beginExpressionEdit(seriesId)
    }
    onVisibleChanged: if (visible) beginEditing()
    onAccepted: settingsVm.updateComputedSeries(seriesId, seriesName, seriesColor, seriesWidth, seriesEnabled, editorModel.toDslText())

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                objectName: "copyExpressionButton"
                icon.source: "icons/copy.svg"
                icon.width: 16
                icon.height: 16
                display: AbstractButton.IconOnly
                enabled: root.editorModel && root.editorModel.root !== null
                Accessible.name: qsTr("Copy")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Copy")
                onClicked: root.editorModel.copyToClipboard()
            }
            Button {
                objectName: "pasteExpressionButton"
                icon.source: "icons/clipboard-paste.svg"
                icon.width: 16
                icon.height: 16
                display: AbstractButton.IconOnly
                enabled: root.editorModel !== null
                Accessible.name: qsTr("Paste")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Paste")
                onClicked: pasteError.visible = !root.editorModel.pasteFromClipboard()
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            id: pasteError
            objectName: "pasteErrorLabel"
            Layout.fillWidth: true
            visible: false
            wrapMode: Text.WordWrap
            color: "#b3261e"
            text: qsTr("Clipboard did not contain a valid expression.")
        }

        Loader {
            id: treeEditorLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: root.editorModel !== null
            sourceComponent: Component {
                ExpressionTreeEditor {
                    objectName: "expressionEditorTreeEditor"
                    model: root.editorModel
                }
            }
        }
    }
}
