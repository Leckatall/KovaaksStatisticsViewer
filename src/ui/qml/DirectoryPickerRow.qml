import QtQuick.Controls
import QtQuick.Layouts

// A labeled, read-only directory path field with a "Browse..." button,
// shared by SettingsDialog's directory-setting categories.
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
        color: "white"
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            objectName: root.objectNamePrefix + "Field"
            Layout.fillWidth: true
            readOnly: true
            text: root.dir.toString().replace("file:///", "")
        }
        Button {
            objectName: root.objectNamePrefix + "BrowseButton"
            text: "Browse..."
            onClicked: root.browseRequested()
        }
    }
}
