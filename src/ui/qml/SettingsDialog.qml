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

    FileDialog {
        id: profileFileDialog
        title: "Choose where to save the profile"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pb"
        nameFilters: ["Profile cache (*.pb)", "All files (*)"]
        selectedFile: root.settingsVm.profilePath
        onAccepted: root.settingsVm.setProfilePath(profileFileDialog.selectedFile)
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
                objectName: "categoryItem_Directories"
                Layout.fillWidth: true
                text: "Directories"
                highlighted: root.currentCategory === 0
                onClicked: root.currentCategory = 0
            }
            ItemDelegate {
                objectName: "categoryItem_Graph Lines"
                Layout.fillWidth: true
                text: "Graph Lines"
                highlighted: root.currentCategory === 1
                onClicked: root.currentCategory = 1
            }
            Item { Layout.fillHeight: true }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentCategory

            // Directories
            ColumnLayout {
                spacing: 16

                DirectoryPickerRow {
                    label: "Kovaaks Directory"
                    dir: root.settingsVm.kovaaksDir
                    objectNamePrefix: "kovaaksDir"
                    onBrowseRequested: kovaaksFolderDialog.open()
                }

                DirectoryPickerRow {
                    label: "Profile Save File"
                    dir: root.settingsVm.profilePath
                    objectNamePrefix: "profilePath"
                    onBrowseRequested: profileFileDialog.open()
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
                        checked: !!columnVisibility[root.graphVm.columnName(modelData).toLowerCase()]
                        onToggled: columnVisibility[root.graphVm.columnName(modelData).toLowerCase()] = checked

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
