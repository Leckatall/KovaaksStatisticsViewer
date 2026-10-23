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
            onClicked: {
                const result = root.settingsVm.save()
                if (!result.accepted) root.focusFirstIssue(result.issues)
            }
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
    readonly property int profileCategory: 0
    readonly property int graphLinesCategory: 1
    readonly property int graphsCategory: 2
    readonly property alias discardChangesPrompt: discardChangesPrompt
    readonly property alias rebuildProfilePrompt: rebuildProfilePrompt

    function open() {
        root.settingsVm.beginVisit()
        if (!visible) _fitToScreen()
        visible = true
        raise()
        requestActivate()
    }

    // Takes keyboard focus to the control an issue concerns, opening its page and graph first.
    function focusIssue(issue) {
        if (!issue) return
        if (issue.group === "graphLines") {
            currentCategory = graphLinesCategory
            return
        }
        if (GraphCatalog.isKnown(issue.graph)) root.settingsVm.selectedGraph = issue.graph
        currentCategory = graphsCategory
        Qt.callLater(() => graphEditor.focusIssue(issue))
    }

    function focusFirstIssue(issues) {
        focusIssue(GraphCatalog.sortedIssues(issues || [])[0])
    }

    function openGraphLines() {
        currentCategory = graphLinesCategory
        open()
    }

    function openProfile() {
        currentCategory = profileCategory
        open()
    }

    // Opens on the Graphs page with `graphKey` selected and keyboard focus in its editor. The visit
    // seeds from the committed configuration, which already holds every quick change.
    function openGraph(graphKey) {
        if (GraphCatalog.isKnown(graphKey)) root.settingsVm.selectedGraph = graphKey
        currentCategory = graphsCategory
        open()
        Qt.callLater(() => { if (!graphEditor.focusFirstControl()) graphSelector.forceActiveFocus() })
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
                    const saved = root.settingsVm.save()
                    if (!saved.accepted) {
                        root.focusFirstIssue(saved.issues)
                        return
                    }
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

    MessageDialog {
        id: rebuildProfilePrompt
        objectName: "rebuildProfilePrompt"
        title: "Rebuild profile?"
        text: "Rebuild re-reads every source file from scratch."
        informativeText: "Runs whose source files have been moved or deleted will be removed from your profile. This cannot be undone."
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onButtonClicked: function (button) {
            if (button === MessageDialog.Ok) root.sessionVm.generateProfile()
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
                    id: profilePage
                    spacing: 16

                    readonly property bool importBusy: root.sessionVm.profileImportBusy === true
                    readonly property string importPhase: root.sessionVm.profileImportPhase || "idle"
                    readonly property string importOutcome: root.sessionVm.profileImportOutcome || "none"
                    readonly property var issues: root.sessionVm.profileImportIssues || []
                    readonly property int added: root.sessionVm.profileImportAdded || 0
                    readonly property int enriched: root.sessionVm.profileImportEnriched || 0
                    readonly property bool saveFailed: root.sessionVm.profileImportSaveFailed === true
                    readonly property bool resultIsError: !importBusy && (saveFailed || importOutcome === "failed")
                    readonly property string resultText: {
                        if (importBusy) {
                            if (importPhase === "saving") return qsTr("Saving…")
                            if (root.sessionVm.profileImportTotal === 0) return qsTr("Scanning sources…")
                            return qsTr("%1 of %2 runs").arg(root.sessionVm.profileImportDone).arg(root.sessionVm.profileImportTotal)
                        }
                        if (saveFailed) return qsTr("Save failed — previous profile unchanged")
                        if (importOutcome === "failed") return qsTr("Refresh failed")
                        if (importOutcome === "none" || importOutcome === "stale") return ""
                        const parts = []
                        if (added > 0) parts.push(qsTr("Added %1").arg(runCount(added)))
                        if (enriched > 0) parts.push(qsTr("updated %1").arg(runCount(enriched)))
                        if (parts.length === 0) return issues.length === 0 ? qsTr("Up to date") : qsTr("No new runs imported")
                        const joined = parts.join("; ")
                        return joined.charAt(0).toUpperCase() + joined.slice(1)
                    }

                    function runCount(count) { return count === 1 ? qsTr("%1 run").arg(count) : qsTr("%1 runs").arg(count) }

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
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: width / 2
                            color: root.sessionVm.profileImportBusy ? "#FFB300"
                                 : root.settingsVm.profileLoaded ? Theme.success : Theme.error
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Label {
                            objectName: "profileLoadedLabel"
                            // The loaded flag describes the profile as it stood before the rebuild,
                            // so it is not shown while one is running.
                            text: "Profile status: " + (root.sessionVm.profileImportBusy ?
                                  (root.sessionVm.profileImportMode === "rebuild" ? "Rebuilding" : "Refreshing")
                                  : root.settingsVm.profileLoaded ? "Loaded" : "Not loaded")
                        }
                        Label {
                            objectName: "profileResultLabel"
                            Layout.fillWidth: true
                            visible: text !== ""
                            elide: Text.ElideRight
                            color: profilePage.resultIsError ? Theme.errorText : Qt.alpha(root.palette.text, 0.7)
                            text: profilePage.resultText
                        }
                    }

                    ProgressBar {
                        objectName: "profileBuildProgressBar"
                        Layout.fillWidth: true
                        Layout.topMargin: -8
                        visible: profilePage.importBusy
                        indeterminate: profilePage.importPhase === "scanning" || profilePage.importPhase === "saving"
                                       || root.sessionVm.profileImportTotal === 0
                        value: root.sessionVm.profileImportProgress || 0
                    }

                    RowLayout {
                        spacing: 8
                        Button {
                            objectName: "refreshProfileButton"
                            text: qsTr("Refresh Profile")
                            enabled: root.settingsVm.kovaaksDirValid === true && !root.sessionVm.profileImportBusy
                            onClicked: root.sessionVm.refreshProfile()
                        }
                        Button {
                            objectName: "generateProfileButton"
                            flat: true
                            text: qsTr("Rebuild from Scratch…")
                            enabled: !root.sessionVm.profileImportBusy
                            onClicked: rebuildProfilePrompt.open()
                        }
                    }

                    Label {
                        objectName: "profileActionsHint"
                        Layout.fillWidth: true
                        Layout.topMargin: -8
                        wrapMode: Text.Wrap
                        color: Qt.alpha(root.palette.text, 0.7)
                        text: qsTr("Refresh imports only new source files. Rebuild re-reads every source file from scratch and drops runs whose files have been moved or deleted.")
                    }

                    Frame {
                        objectName: "profileIssuesFrame"
                        Layout.fillWidth: true
                        visible: profilePage.issues.length > 0
                        ColumnLayout {
                            width: parent.width
                            spacing: 6
                            Label {
                                objectName: "profileIssuesHeader"
                                Layout.fillWidth: true
                                font.bold: true
                                text: profilePage.issues.length === 1 ? qsTr("1 source file could not be imported")
                                    : qsTr("%1 source files could not be imported").arg(profilePage.issues.length)
                            }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(issueText.implicitHeight, 160)
                                TextArea {
                                    id: issueText
                                    objectName: "profileIssueText"
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: TextEdit.NoWrap
                                    padding: 0
                                    background: null
                                    text: profilePage.issues.map(issue => {
                                        const fileName = (issue.path || "").split(/[\\/]/).pop()
                                        return fileName === "" ? issue.message
                                            : qsTr("%1: %2").arg(fileName).arg(issue.message)
                                    }).join("\n")
                                }
                            }
                        }
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

                        GraphSettingsEditor {
                            id: graphEditor
                            anchors.fill: parent
                            settingsVm: root.settingsVm
                            graphKey: root.settingsVm.selectedGraph
                            onGraphLinesRequested: root.currentCategory = root.graphLinesCategory
                            onIssueActivated: issue => root.focusIssue(issue)
                        }
                    }
                }
            }
        }
    }
}
