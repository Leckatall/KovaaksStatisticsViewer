import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// A labelled text field with its unit, its stated limits and its inline error. The error label is
// `<fieldName>_error` so a test or screen reader finds it beside the control it belongs to.
ColumnLayout {
    id: root

    required property string fieldName
    property string graphKey
    property string label
    property string unit
    property string hint
    property string placeholder
    property string shown
    property string errorText

    readonly property alias input: input

    signal edited(string text)

    spacing: 2

    Label {
        text: root.label
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        GraphTextInput {
            id: input
            objectName: root.fieldName
            Layout.fillWidth: true
            Layout.minimumWidth: 72
            graphKey: root.graphKey
            shown: root.shown
            placeholderText: root.placeholder
            selectByMouse: true
            Accessible.name: root.unit !== "" ? qsTr("%1 (%2)").arg(root.label).arg(root.unit) : root.label
            Accessible.description: root.errorText !== "" ? root.errorText + " " + root.hint : root.hint
            onEdited: text => root.edited(text)
        }
        Label {
            objectName: root.fieldName + "_unit"
            visible: text !== ""
            text: root.unit
        }
    }

    Label {
        objectName: root.fieldName + "_hint"
        Layout.fillWidth: true
        visible: text !== ""
        text: root.hint
        wrapMode: Text.WordWrap
        opacity: 0.75
    }

    Label {
        objectName: root.fieldName + "_error"
        Layout.fillWidth: true
        visible: text !== ""
        text: root.errorText
        color: Theme.errorText
        wrapMode: Text.WordWrap
    }
}
