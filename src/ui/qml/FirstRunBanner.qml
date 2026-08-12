import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    signal chooseFolderRequested()

    contentItem: RowLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: "Choose your Kovaaks folder to start viewing your statistics."
            wrapMode: Text.Wrap
        }

        Button {
            objectName: "chooseKovaaksFolderButton"
            text: "Choose Kovaaks Folder..."
            onClicked: root.chooseFolderRequested()
        }
    }
}
