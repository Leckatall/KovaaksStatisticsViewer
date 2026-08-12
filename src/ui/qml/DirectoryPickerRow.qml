import QtQuick.Controls
import QtQuick.Layouts

// A labeled, read-only path field (directory or file) with a "Browse..."
// button, shared by SettingsDialog's path-setting rows.
ColumnLayout {
    id: root
    spacing: 6

    property alias label: titleLabel.text
    required property url dir
    required property string objectNamePrefix
    signal browseRequested()

    Label {
        id: titleLabel
        font.bold: true
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        TextField {
            objectName: root.objectNamePrefix + "Field"
            Layout.fillWidth: true
            readOnly: true
            text: root.dir.toString().replace("file:///", "")
            ToolTip.visible: hovered && text.length > 0
            ToolTip.text: text
        }
        Button {
            objectName: root.objectNamePrefix + "BrowseButton"
            text: "Browse…"
            flat: true
            onClicked: root.browseRequested()
        }
    }
}
