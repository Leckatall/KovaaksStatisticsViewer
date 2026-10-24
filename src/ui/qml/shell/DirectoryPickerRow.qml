import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// A labelled, read-only path field (directory or file) with a folder
// icon browse button, shared by SettingsDialog's path-setting rows.
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
        // A path is read, not typed, so it wraps instead of scrolling: a
        // single-line field hides one end of a long path, but a wrapped one shows
        // the whole path (and so always its final folder or file name) at once.
        // Still selectable, so the path can be copied.
        TextArea {
            objectName: root.objectNamePrefix + "Field"
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
            placeholderText: qsTr("Not set")
            text: root.dir.toString().replace("file:///", "")
            Accessible.name: root.label
        }
        ChromeIconButton {
            objectName: root.objectNamePrefix + "BrowseButton"
            Layout.alignment: Qt.AlignTop
            icon.source: "../icons/folder-open.svg"
            Accessible.name: qsTr("Browse…")
            ToolTip.text: qsTr("Browse…")
            onClicked: root.browseRequested()
        }
    }
}
