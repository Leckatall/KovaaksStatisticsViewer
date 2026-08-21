import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    objectName: "settingsDialog"
    title: "Settings"
    flags: Qt.Dialog
    modality: Qt.WindowModal
    width: 680
    height: 460

    required property var settingsVm
    required property var sessionVm
    required property var visualSettings

    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
    }

    function seriesName(id) {
        const series = root.settingsVm.allSeriesConfigs.find(s => s.id === id)
        return series ? series.name : id
    }

    property int currentCategory: 0
    readonly property int graphLinesCategory: 1
    readonly property alias discardChangesPrompt: discardChangesPrompt

    function open() {
        root.settingsVm.beginSeriesDraft()
        visible = true
        raise()
        requestActivate()
    }

    function openGraphLines() {
        currentCategory = graphLinesCategory
        open()
    }

    onClosing: (close) => {
        if (root.settingsVm.pendingChanges) {
            close.accepted = false
            discardChangesPrompt.open()
        }
    }

    MessageDialog {
        id: discardChangesPrompt
        objectName: "discardChangesPrompt"
        title: "Save changes?"
        text: "You have unsaved changes."
        informativeText: "Do you want to save them before closing?"
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: function (button) {
            switch (button) {
                case MessageDialog.Save:
                    root.settingsVm.commitSeriesDraft()
                    break
                case MessageDialog.Discard:
                    root.settingsVm.discardSeriesDraft()
                    break
                case MessageDialog.Cancel:
                    return
            }
            root.close()
        }
    }

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
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

                    readonly property var enabledSeriesConfigs: root.settingsVm.allSeriesConfigs.filter(s => s.enabled)

                    readonly property var visibleAxisColumns: {
                        const seenPositions = new Set()
                        const result = []
                        for (const series of graphLinesPage.enabledSeriesConfigs) {
                            if (!seenPositions.has(series.displayPosition)) {
                                seenPositions.add(series.displayPosition)
                                result.push(series.id)
                            }
                        }
                        return result
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
                            enabled: graphLinesPage.visibleAxisColumns.length > 0
                            model: graphLinesPage.visibleAxisColumns
                            displayText: enabled ? root.seriesName(graphLinesPage.visibleAxisColumns[currentIndex]) : ""
                            delegate: ItemDelegate {
                                required property var modelData
                                width: yAxisColumnComboBox.width
                                text: root.seriesName(modelData)
                            }
                            currentIndex: {
                                const idx = graphLinesPage.visibleAxisColumns.indexOf(root.visualSettings.graphAxisSeriesId)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: index => root.visualSettings.graphAxisSeriesId = graphLinesPage.visibleAxisColumns[index]
                        }
                    }

                    SeriesConfigDraftPanel {
                        Layout.fillWidth: true
                        settingsVm: root.settingsVm
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
