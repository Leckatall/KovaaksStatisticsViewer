import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Settings"
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: 640
    height: 420

    required property var settingsVm
    required property var graphVm
    required property var columnVisibility

    property int currentCategory: 0

    FolderDialog {
        id: kovaaksFolderDialog
        currentFolder: root.settingsVm.kovaaksDir
        onAccepted: root.settingsVm.setKovaaksDir(kovaaksFolderDialog.selectedFolder)
    }

    FolderDialog {
        id: profileFolderDialog
        currentFolder: root.settingsVm.profileDir
        onAccepted: root.settingsVm.setProfileDir(profileFolderDialog.selectedFolder)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 16

        ColumnLayout {
            id: categoryList
            objectName: "categoryList"
            Layout.preferredWidth: 160
            Layout.fillHeight: true
            spacing: 0

            ItemDelegate {
                objectName: "categoryItem_Kovaaks Directory"
                Layout.fillWidth: true
                text: "Kovaaks Directory"
                highlighted: root.currentCategory === 0
                onClicked: root.currentCategory = 0
            }
            ItemDelegate {
                objectName: "categoryItem_Profile"
                Layout.fillWidth: true
                text: "Profile"
                highlighted: root.currentCategory === 1
                onClicked: root.currentCategory = 1
            }
            ItemDelegate {
                objectName: "categoryItem_Graph Lines"
                Layout.fillWidth: true
                text: "Graph Lines"
                highlighted: root.currentCategory === 2
                onClicked: root.currentCategory = 2
            }
            Item { Layout.fillHeight: true }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentCategory

            // Kovaaks Directory
            ColumnLayout {
                spacing: 6

                DirectoryPickerRow {
                    label: "Kovaaks Directory"
                    dir: root.settingsVm.kovaaksDir
                    objectNamePrefix: "kovaaksDir"
                    onBrowseRequested: kovaaksFolderDialog.open()
                }
                Item { Layout.fillHeight: true }
            }

            // Profile
            ColumnLayout {
                spacing: 6

                DirectoryPickerRow {
                    label: "Profile Save Location"
                    dir: root.settingsVm.profileDir
                    objectNamePrefix: "profileDir"
                    onBrowseRequested: profileFolderDialog.open()
                }
                Label {
                    objectName: "profileLoadedLabel"
                    text: "Profile status: " + (root.settingsVm.profileLoaded ? "Loaded" : "Not loaded")
                    color: root.settingsVm.profileLoaded ? "#4CAF50" : "#F44336"
                }
                Item { Layout.fillHeight: true }
            }

            // Graph Lines
            ColumnLayout {
                spacing: 6

                Label {
                    text: "Visible Graph Lines"
                    font.bold: true
                    color: "white"
                }
                Repeater {
                    model: root.graphVm.plottableColumns

                    CheckBox {
                        required property int modelData
                        objectName: "columnVisibilityCheckBox_" + root.graphVm.columnName(modelData)
                        Layout.fillWidth: true

                        text: root.graphVm.columnName(modelData)
                        checked: root.columnVisibility[root.graphVm.columnName(modelData).toLowerCase()]
                        onToggled: root.columnVisibility[root.graphVm.columnName(modelData).toLowerCase()] = checked

                        background: Rectangle {
                            anchors.fill: parent
                            color: root.graphVm.columnColor(modelData)
                            opacity: 0.5
                            radius: 5
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
