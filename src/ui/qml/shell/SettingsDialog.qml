import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import KovaaksStatsViewer
import "../graph/GraphCatalog.js" as GraphCatalog

ApplicationWindow {
    id: root
    objectName: "settingsDialog"
    title: "Settings"
    flags: Qt.Dialog
    modality: Qt.WindowModal
    width: defaultWidth
    height: defaultHeight
    minimumWidth: Math.min(floorWidth, _limitWidth())
    minimumHeight: Math.min(floorHeight, _limitHeight())

    // Room for the Graph Lines list and full paths. It opens at the size it was last left at
    // (window/settingsDialog*), never below the floor, and shrinks below the floor only when the
    // main window and the screen are both too small for it.
    readonly property int defaultWidth: 860
    readonly property int defaultHeight: 620
    readonly property int floorWidth: 680
    readonly property int floorHeight: 460

    Settings {
        id: sizeSettings
        category: "window"
        property int settingsDialogWidth: 0
        property int settingsDialogHeight: 0
    }

    function _limitWidth() {
        return Math.max(root.screen ? root.screen.desktopAvailableWidth : 0,
                        root.transientParent ? root.transientParent.width : 0) || root.defaultWidth
    }
    function _limitHeight() {
        return Math.max(root.screen ? root.screen.desktopAvailableHeight : 0,
                        root.transientParent ? root.transientParent.height : 0) || root.defaultHeight
    }

    function _fitToScreen() {
        const wantWidth = sizeSettings.settingsDialogWidth > 0 ? sizeSettings.settingsDialogWidth : defaultWidth
        const wantHeight = sizeSettings.settingsDialogHeight > 0 ? sizeSettings.settingsDialogHeight : defaultHeight
        width = Math.min(Math.max(wantWidth, floorWidth), _limitWidth())
        height = Math.min(Math.max(wantHeight, floorHeight), _limitHeight())
        // Centred over the main window, kept on its screen.
        const owner = root.transientParent
        if (!owner) return
        const left = root.screen ? root.screen.virtualX : 0
        const top = root.screen ? root.screen.virtualY : 0
        x = Math.max(left, owner.x + Math.round((owner.width - width) / 2))
        y = Math.max(top, owner.y + Math.round((owner.height - height) / 2))
    }

    onVisibleChanged: {
        if (visible) return
        sizeSettings.settingsDialogWidth = width
        sizeSettings.settingsDialogHeight = height
    }

    required property var settingsVm
    required property var sessionVm

    // One working copy per visit: Save keeps the window open and clean, Cancel drops every edit
    // since the visit began or last saved and closes.
    footer: DialogButtonBox {
        Button {
            objectName: "settingsSaveButton"
            text: qsTr("Save")
            enabled: root.settingsVm.pendingChanges
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            onClicked: root.settingsVm.save()
        }
        Button {
            objectName: "settingsCancelButton"
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: {
                root.settingsVm.endVisit()
                root.close()
            }
        }
    }

    function seriesName(id) {
        const series = root.settingsVm.allSeriesConfigs.find(s => s.id === id)
        return series ? series.name : id
    }

    property int currentCategory: 0
    readonly property int graphLinesCategory: 1
    readonly property int graphsCategory: 2
    readonly property alias discardChangesPrompt: discardChangesPrompt

    function open() {
        root.settingsVm.beginVisit()
        if (!visible) _fitToScreen()
        visible = true
        raise()
        requestActivate()
    }

    function openGraphLines() {
        currentCategory = graphLinesCategory
        open()
    }

    // Opens on the Graphs page with `graphKey` selected and keyboard focus in its editor. The visit
    // seeds from the committed configuration, which already holds every quick change.
    function openGraph(graphKey) {
        if (GraphCatalog.isKnown(graphKey)) root.settingsVm.selectedGraph = graphKey
        currentCategory = graphsCategory
        open()
        graphSelector.forceActiveFocus()
    }

    onClosing: (close) => {
        if (root.settingsVm.pendingChanges) {
            close.accepted = false
            discardChangesPrompt.open()
            return
        }
        root.settingsVm.endVisit()
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
                    // A rejected Save keeps the window, its working copy and the issues to correct.
                    if (!root.settingsVm.save().accepted)
                        return
                    break
                case MessageDialog.Discard:
                    root.settingsVm.endVisit()
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
                    CategoryButton {
                        objectName: "categoryItem_Graphs"
                        Layout.fillWidth: true
                        text: "Graphs"
                        highlighted: root.currentCategory === root.graphsCategory
                        onClicked: root.currentCategory = root.graphsCategory
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
                            color: root.sessionVm.profileBuildInProgress ? "#FFB300"
                                 : root.settingsVm.profileLoaded ? Theme.success : Theme.error
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            objectName: "profileLoadedLabel"
                            // The loaded flag describes the profile as it stood before the rebuild,
                            // so it is not shown while one is running.
                            text: "Profile status: " + (root.sessionVm.profileBuildInProgress ? "Rebuilding"
                                  : root.settingsVm.profileLoaded ? "Loaded" : "Not loaded")
                        }
                    }

                    Button {
                        objectName: "generateProfileButton"
                        text: "Rebuild Profile from Kovaaks Directory"
                        enabled: !root.sessionVm.profileBuildInProgress
                        onClicked: root.sessionVm.generateProfile()
                    }

                    Label {
                        objectName: "profileBuildProgressLabel"
                        visible: root.sessionVm.profileBuildInProgress
                        // The file count only arrives with the first per-file report.
                        text: root.sessionVm.profileBuildTotal === 0
                              ? "Scanning Kovaaks Directory..."
                              : "Rebuilding profile: " + root.sessionVm.profileBuildDone
                                + " of " + root.sessionVm.profileBuildTotal + " files"
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
                                const idx = graphLinesPage.visibleAxisColumns.indexOf(root.settingsVm.runAxisLabelSeriesId)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: index => root.settingsVm.setRunAxisLabelSeriesId(graphLinesPage.visibleAxisColumns[index])
                        }
                    }

                    SeriesConfigDraftPanel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        settingsVm: root.settingsVm
                    }
                }

                // Graphs
                ColumnLayout {
                    id: graphsPage
                    objectName: "graphsSettingsPage"
                    spacing: 8

                    Label {
                        text: "Graphs"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.bottomMargin: 4
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            text: "Graph"
                            Layout.alignment: Qt.AlignVCenter
                        }
                        ComboBox {
                            id: graphSelector
                            objectName: "graphSelector"
                            Layout.fillWidth: true
                            model: GraphCatalog.graphs
                            textRole: "name"
                            currentIndex: Math.max(0, GraphCatalog.graphs.findIndex(g => g.key === root.settingsVm.selectedGraph))
                            Accessible.name: qsTr("Graph")
                            onActivated: index => {
                                root.settingsVm.selectedGraph = GraphCatalog.graphs[index].key
                                graphSelector.currentIndex = Qt.binding(
                                    () => Math.max(0, GraphCatalog.graphs.findIndex(g => g.key === root.settingsVm.selectedGraph)))
                            }
                        }
                    }

                    Item {
                        objectName: "graphEditorArea"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
}
