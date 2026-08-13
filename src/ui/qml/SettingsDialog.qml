import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    objectName: "settingsDialog"
    title: "Settings"
    modal: false
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: 680
    height: 460
    padding: 0
    margins: 10

    required property var settingsVm
    required property var sessionVm
    required property var graphVm
    required property var columnVisibility
    required property var graphAxisSettings

    property int currentCategory: 0

    // Rounded "pill" navigation entry for the sidebar.
    component CategoryButton: ItemDelegate {
        id: catButton
        padding: 10
        Layout.fillWidth: true

        background: Rectangle {
            radius: 2
            color: catButton.highlighted
                   ? Qt.alpha(catButton.palette.accent, 0.3)
                   : (catButton.hovered ? Qt.alpha(catButton.palette.text, 0.06) : "transparent")
        }
        contentItem: Label {
            text: catButton.text
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
        nameFilters: ["Profile (*.pb)", "All files (*)"]
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
            color: Qt.darker(root.palette.window, 1.2)


            // Right-edge divider between sidebar and content.
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: root.palette.mid
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
                    text: "Profile"
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
                    text: "Profile"
                    font.pixelSize: 18
                    font.bold: true
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
                        // Semantic status, not chrome — there is no palette role
                        // for ok/error, so these stay literal.
                        color: root.settingsVm.profileLoaded ? "#4CAF50" : "#E53935"
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Label {
                        objectName: "profileLoadedLabel"
                        text: "Profile status: " + (root.settingsVm.profileLoaded ? "Loaded" : "Not loaded")
                    }
                }

                Button {
                    objectName: "generateProfileButton"
                    text: "Generate Profile from current kovaaks dir"
                    enabled: !root.sessionVm.profileBuildInProgress
                    onClicked: root.sessionVm.generateProfile()
                }

                ProgressBar {
                    objectName: "profileBuildProgressBar"
                    Layout.fillWidth: true
                    visible: root.sessionVm.profileBuildInProgress
                    // The file count only arrives with the first per-file report; until then
                    // there is nothing to show a fraction of.
                    indeterminate: value === 0
                    value: root.sessionVm.profileBuildProgress
                }
                Item { Layout.fillHeight: true }
            }

            // Graph Lines
            ColumnLayout {
                id: graphLinesPage
                spacing: 8

                readonly property var visibleColumns: {
                    const cols = root.graphVm.plottableColumns;
                    const result = [];
                    for (let i = 0; i < cols.length; i++) {
                        if (root.columnVisibility[root.graphVm.columnKey(cols[i])]) {
                            result.push(cols[i]);
                        }
                    }
                    return result;
                }

                Label {
                    text: "Graph Lines"
                    font.pixelSize: 18
                    font.bold: true
                    Layout.bottomMargin: 4
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        text: "Y-axis labels"
                        Layout.alignment: Qt.AlignVCenter
                    }
                    ComboBox {
                        id: yAxisColumnComboBox
                        objectName: "yAxisColumnComboBox"
                        Layout.fillWidth: true
                        enabled: graphLinesPage.visibleColumns.length > 0
                        model: graphLinesPage.visibleColumns
                        displayText: enabled ? root.graphVm.columnName(graphLinesPage.visibleColumns[currentIndex]) : ""
                        delegate: ItemDelegate {
                            required property int modelData
                            width: yAxisColumnComboBox.width
                            text: root.graphVm.columnName(modelData)
                        }
                        currentIndex: {
                            const idx = graphLinesPage.visibleColumns.findIndex(
                                c => root.graphVm.columnKey(c) === root.graphAxisSettings.yAxisColumnKey);
                            return idx >= 0 ? idx : 0;
                        }
                        onActivated: index => {
                            root.graphAxisSettings.yAxisColumnKey = root.graphVm.columnKey(graphLinesPage.visibleColumns[index]);
                        }
                    }
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
                            border.color: root.palette.mid
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            text: root.graphVm.columnName(lineRow.modelData)
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
