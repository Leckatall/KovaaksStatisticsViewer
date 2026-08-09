import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    objectName: "settingsDialog"
    title: "Settings"
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: 680
    height: 460
    padding: 0
    margins: 10

    required property var settingsVm
    required property var graphVm
    required property var columnVisibility

    property int currentCategory: 0

    // Rounded "pill" navigation entry for the sidebar.
    component CategoryButton: ItemDelegate {
        id: catButton
        padding: 10
        Layout.fillWidth: true

        background: Rectangle {
            radius: 2
            color: catButton.highlighted
                   ? Qt.alpha(Material.accentColor, 0.22)
                   : (catButton.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
        }
        contentItem: Label {
            text: catButton.text
            color: Material.foreground
            font.bold: catButton.highlighted
            verticalAlignment: Text.AlignVCenter
        }
    }

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
        spacing: 10

        // ---- Sidebar ------------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 176
            Layout.fillHeight: true
            color: Qt.darker(Material.dialogColor, 1.25)


            // Right-edge divider between sidebar and content.
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            ColumnLayout {
                id: categoryList
                objectName: "categoryList"
                anchors.fill: parent
                anchors.margins: 5
                anchors.topMargin: 5
                spacing: 4

                CategoryButton {
                    objectName: "categoryItem_Directories"
                    Layout.fillWidth: true
                    text: "Directories"
                    highlighted: root.currentCategory === 0
                    onClicked: root.currentCategory = 0
                }
                CategoryButton {
                    objectName: "categoryItem_Graph Lines"
                    Layout.fillWidth: true
                    text: "Graph Lines"
                    highlighted: root.currentCategory === 1
                    onClicked: root.currentCategory = 1
                }
                Item { Layout.fillHeight: true }
            }
        }

        // ---- Content ------------------------------------------------------
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentCategory

            // Directories
            ColumnLayout {
                spacing: 16

                Label {
                    text: "Directories"
                    font.pixelSize: 18
                    font.bold: true
                    color: Material.foreground
                    Layout.bottomMargin: 4
                }

                DirectoryPickerRow {
                    Layout.fillWidth: true
                    label: "Kovaaks Directory"
                    dir: root.settingsVm.kovaaksDir
                    objectNamePrefix: "kovaaksDir"
                    onBrowseRequested: kovaaksFolderDialog.open()
                }

                DirectoryPickerRow {
                    Layout.fillWidth: true
                    label: "Profile Save File"
                    dir: root.settingsVm.profilePath
                    objectNamePrefix: "profilePath"
                    onBrowseRequested: profileFileDialog.open()
                }

                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 10
                        height: 10
                        radius: width / 2
                        color: root.settingsVm.profileLoaded
                               ? Material.color(Material.Green)
                               : Material.color(Material.Red)
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Label {
                        objectName: "profileLoadedLabel"
                        text: "Profile status: " + (root.settingsVm.profileLoaded ? "Loaded" : "Not loaded")
                        color: Material.foreground
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // Graph Lines
            ColumnLayout {
                spacing: 8

                Label {
                    text: "Graph Lines"
                    font.pixelSize: 18
                    font.bold: true
                    color: Material.foreground
                    Layout.bottomMargin: 4
                }

                Repeater {
                    model: root.graphVm.plottableColumns

                    RowLayout {
                        id: lineRow
                        required property int modelData
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            width: 16
                            height: 16
                            radius: 4
                            color: root.graphVm.columnColor(lineRow.modelData)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.15)
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: root.graphVm.columnName(lineRow.modelData)
                            color: Material.foreground
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Item { Layout.fillWidth: true }
                        Switch {
                            objectName: "columnVisibilityCheckBox_" + root.graphVm.columnName(lineRow.modelData)
                            checked: !!columnVisibility[root.graphVm.columnKey(lineRow.modelData)]
                            onToggled: columnVisibility[root.graphVm.columnKey(lineRow.modelData)] = checked
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }


}
