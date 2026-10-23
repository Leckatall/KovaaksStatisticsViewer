import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    signal chooseFolderRequested()

    contentItem: RowLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Your current Kovaaks directory is invalid. Please change it.")
            wrapMode: Text.Wrap
        }

        Button {
            objectName: "chooseKovaaksFolderButton"
            text: qsTr("Choose Kovaaks Folder...")
            onClicked: root.chooseFolderRequested()
        }
    }
}
