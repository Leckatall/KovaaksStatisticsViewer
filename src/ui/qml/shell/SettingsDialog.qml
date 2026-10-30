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
    palette: Theme.palette
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
        sizeSettings.setValue("settingsDialogPage", currentPage)
        sizeSettings.setValue("settingsDialogSection", currentSection)
        sizeSettings.sync()
    }

    required property var settingsVm
    required property var sessionVm
    property var visualSettings: null

    // One working copy per visit: Save keeps the window open and clean, Cancel drops every edit
    // since the visit began or last saved and closes.
    footer: RowLayout {
        Label {
            objectName: "settingsStatusLabel"
            Layout.fillWidth: true
            Layout.leftMargin: 10
            elide: Text.ElideRight
            color: root.saveRejected && root.issueGraphs().length ? Theme.errorText : root.palette.text
            text: {
                const issueNames = root.graphNames(root.issueGraphs())
                const changedNames = root.graphNames(root.settingsVm.changedGraphs || [])
                if (root.saveRejected && issueNames.length)
                    return qsTr("Nothing was saved: fix the marked settings in %1.").arg(issueNames.join(", "))
                return changedNames.length ? qsTr("Unsaved changes in %1.").arg(changedNames.join(", ")) : ""
            }
        }
        DialogButtonBox {
            Button {
                objectName: "settingsSaveButton"
                text: qsTr("Save")
                enabled: root.settingsVm.pendingChanges || root.hasUncommittedEdit()
                DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
                onClicked: {
                    root.commitActiveEdit()
                    const result = root.settingsVm.save()
                    root.recordSave(result)
                    if (!result.accepted) root.focusFirstIssue(result.issues)
                }
            }
            Button {
                objectName: "settingsCancelButton"
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: { root.settingsVm.endVisit(); root.close() }
            }
        }
    }

    property string currentPage: "profile"
    property string previousPage: "profile"
    property string sectionLatch: ""
    property real latchY: 0
    property bool jumping: false
    readonly property string currentSection: currentPage === "profile" ? "" : (sectionLatch || graphEditor.sectionInView)
    property var leftWithIssue: []
    property var rejectedGraphs: []
    property bool saveRejected: false

    // A rejected Save can name issues the draft does not have, such as one only the store reports.
    function issueGraphs() {
        return [...new Set((settingsVm.draftIssues || []).concat(settingsVm.issues || []).map(i => i.graph))]
    }
    function graphNames(keys) { return GraphCatalog.graphs.filter(g => keys.includes(g.key)).map(g => g.name) }
    function markerFor(key) {
        if (issueGraphs().includes(key) && (leftWithIssue.includes(key) || rejectedGraphs.includes(key))) return "issue"
        return (settingsVm.changedGraphs || []).includes(key) ? "changed" : ""
    }
    function pruneIssues() {
        const live = issueGraphs()
        leftWithIssue = leftWithIssue.filter(k => live.includes(k))
        rejectedGraphs = rejectedGraphs.filter(k => live.includes(k))
        if (!live.length) saveRejected = false
    }
    function recordSave(result) {
        saveRejected = !result.accepted
        if (!result.accepted)
            rejectedGraphs = [...new Set((result.issues || []).map(i => i.graph))]
        pruneIssues()
    }
    onCurrentPageChanged: {
        if (visible && previousPage !== currentPage && issueGraphs().includes(previousPage))
            leftWithIssue = [...new Set(leftWithIssue.concat([previousPage]))]
        previousPage = currentPage
    }
    Connections {
        target: root.settingsVm
        ignoreUnknownSignals: true
        function onDraftIssuesChanged() { root.pruneIssues() }
        function onIssuesChanged() { root.pruneIssues() }
    }
    Connections {
        target: graphEditor.scroller.contentItem
        function onContentYChanged() {
            if (!root.jumping && root.sectionLatch && Math.abs(target.contentY - root.latchY) > 0.5)
                root.sectionLatch = ""
        }
    }
    function jumpToSection(group) {
        if (!graphEditor.sections.includes(group)) return
        jumping = true
        sectionLatch = group
        graphEditor.scrollToSection(group)
        latchY = graphEditor.scroller.contentItem.contentY
        jumping = false
    }
    function selectGraph(key, reset) {
        if (!GraphCatalog.isKnown(key)) return
        const changed = currentPage !== key
        settingsVm.selectedGraph = key
        currentPage = key
        if (changed || reset) {
            sectionLatch = ""
            graphEditor.scroller.contentItem.contentY = 0
        }
    }
    readonly property alias discardChangesPrompt: unsavedChangesPrompt
    readonly property alias rebuildProfilePrompt: rebuildProfilePrompt

    function _show() {
        if (!visible) {
            root.settingsVm.beginVisit()
            leftWithIssue = []
            rejectedGraphs = []
            saveRejected = false
            _fitToScreen()
        }
        visible = true
        raise()
        requestActivate()
    }
    function _restorePlace() {
        const page = String(sizeSettings.value("settingsDialogPage", ""))
        const section = String(sizeSettings.value("settingsDialogSection", ""))
        if (!page || page === "profile") currentPage = "profile"
        else if (GraphCatalog.isKnown(page)) {
            selectGraph(page, true)
            if (graphEditor.sections.includes(section)) Qt.callLater(() => jumpToSection(section))
        } else selectGraph("run", true)
    }
    // The stored place is the previous close's, so an already-open window keeps where it is.
    function open() {
        if (!visible) _restorePlace()
        _show()
    }
    function focusIssue(issue) {
        if (!issue) return
        const graph = issue.group === "graphLines" ? "run" : issue.graph
        selectGraph(graph, false)
        if (issue.group === "graphLines") {
            const axis = /^axis:(.+)$/.exec(issue.target || "")
            if (!axis || issue.field !== "name") {
                Qt.callLater(() => jumpToSection("seriesStyle"))
                return
            }
            // Focus lands before Save returns; the jump then brings the Axes heading into view, and the
            // panel is looked up again in case it was not loaded yet.
            const focusAxisName = () => {
                const panel = GraphCatalog.findByObjectName(graphEditor, "axesPanel")
                if (panel) panel.focusAxisName(axis[1])
            }
            focusAxisName()
            Qt.callLater(() => { jumpToSection("axes"); focusAxisName() })
            return
        }
        Qt.callLater(() => graphEditor.focusIssue(issue))
    }
    function focusFirstIssue(issues) {
        focusIssue(GraphCatalog.sortedIssues(issues || [], settingsVm.graphDrafts, settingsVm.axisOverview)[0])
    }
    function openGraphLines() {
        selectGraph("run", false)
        _show()
        Qt.callLater(() => jumpToSection("seriesStyle"))
    }
    function openProfile() { currentPage = "profile"; _show() }
    function openGraph(graphKey) {
        if (!GraphCatalog.isKnown(graphKey)) { _restorePlace(); _show(); return }
        selectGraph(graphKey, true)
        _show()
        Qt.callLater(() => { if (!graphEditor.focusFirstControl()) focusNav(graphKey) })
    }
    function focusNav(key) {
        const target = GraphCatalog.findByObjectName(categoryList, "settingsNav_" + key)
        if (target) target.forceActiveFocus()
    }

    function moveSidebar(from, direction) {
        const entries = ["profile"]
        for (const graph of GraphCatalog.graphs) {
            if (currentPage === graph.key) {
                for (const section of graphEditor.sections) entries.push(graph.key + "_" + section)
            } else entries.push(graph.key)
        }
        let index = entries.indexOf(from.objectName.substring("settingsNav_".length))
        if (index < 0) index = Math.max(0, entries.indexOf(currentPage + "_" + currentSection))
        index = Math.max(0, Math.min(entries.length - 1, index + direction))
        let key = entries[index]
        if (key === "profile") currentPage = "profile"
        else if (GraphCatalog.isKnown(key)) {
            selectGraph(key, true)
            // Moving up enters the graph from below, at its last section.
            const sections = graphEditor.sections
            const section = sections[direction < 0 ? sections.length - 1 : 0]
            key += "_" + section
            if (direction < 0) Qt.callLater(() => jumpToSection(section))
        }
        else jumpToSection(key.substring(key.indexOf("_") + 1))
        Qt.callLater(() => focusNav(key))
    }

    // Disabling the focused Save button changes activeFocusItem mid-evaluation of its own `enabled`
    // binding; this property only changes when focus enters or leaves a field, which breaks that loop.
    readonly property CommitOnFinishField focusedField: root.activeFocusItem as CommitOnFinishField

    function hasUncommittedEdit(): bool {
        return focusedField !== null && focusedField.uncommittedEdit
    }

    // Line and axis names commit on editingFinished, which closing the window doesn't trigger.
    function commitActiveEdit() {
        if (hasUncommittedEdit())
            focusedField.editingFinished()
    }

    onClosing: (close) => {
        root.commitActiveEdit()
        if (root.settingsVm.pendingChanges) {
            close.accepted = false
            unsavedChangesPrompt.open()
            return
        }
        root.settingsVm.endVisit()
    }

    MessageDialog {
        id: unsavedChangesPrompt
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
                    root.recordSave(saved)
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
        rightPadding: graphHidden && marker !== "" ? 29 : 18
        Layout.fillWidth: true
        focusPolicy: Qt.StrongFocus
        property bool emphasised: false
        property string marker: ""
        property bool graphHidden: false
        Accessible.description: {
            const status = marker === "issue" ? qsTr("Has a setting to fix before saving") :
                           marker === "changed" ? qsTr("Has unsaved changes") : ""
            if (!graphHidden) return status
            return status === "" ? qsTr("Hidden") : qsTr("Hidden. %1").arg(status)
        }
        Keys.onUpPressed: root.moveSidebar(catButton, -1)
        Keys.onDownPressed: root.moveSidebar(catButton, 1)
        onActiveFocusChanged: if (activeFocus) GraphCatalog.ensureVisible(sidebarScroll.contentItem as Flickable, catButton, 0)

        background: Rectangle {
            radius: 2
            color: catButton.highlighted
                   ? Qt.alpha(catButton.palette.accent, 0.3)
                   : (catButton.hovered ? Qt.alpha(catButton.palette.text, 0.06) : "transparent")
        }
        contentItem: Label {
            text: catButton.text
            font.bold: catButton.highlighted || catButton.emphasised
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        // Sits in the right padding the marker dot reserves, so hiding a graph leaves its name's size alone.
        // Only an entry with both steps the icon inward, past the dot.
        Image {
            objectName: "hiddenStatus_" + catButton.objectName
            anchors.right: parent.right
            anchors.rightMargin: catButton.marker !== "" ? 15 : 4
            anchors.verticalCenter: parent.verticalCenter
            visible: catButton.graphHidden
            source: "../icons/eye-off.svg"
            sourceSize.width: 14
            sourceSize.height: 14
            Accessible.ignored: true
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

                ScrollView {
                    id: sidebarScroll
                    anchors.fill: parent
                    anchors.margins: 5
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        id: categoryList
                        objectName: "categoryList"
                        width: sidebarScroll.availableWidth
                        spacing: 4
                        CategoryButton {
                            objectName: "settingsNav_profile"
                            text: qsTr("Profile")
                            highlighted: root.currentPage === "profile"
                            onClicked: root.currentPage = "profile"
                        }
                        Label {
                            objectName: "settingsNavHeading"
                            text: qsTr("GRAPHS")
                            font.bold: true
                            color: Qt.alpha(root.palette.text, 0.78)
                            Accessible.role: Accessible.Heading
                        }
                        Repeater {
                            model: GraphCatalog.graphs
                            delegate: ColumnLayout {
                                id: graphEntry
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 2
                                CategoryButton {
                                    id: graphButton
                                    objectName: "settingsNav_" + graphEntry.modelData.key
                                    text: graphEntry.modelData.name
                                    emphasised: root.currentPage === graphEntry.modelData.key
                                    marker: root.markerFor(graphEntry.modelData.key)
                                    graphHidden: root.visualSettings !== null
                                                 && !root.visualSettings.isGraphVisible(graphEntry.modelData.key)
                                    onClicked: root.selectGraph(graphEntry.modelData.key, true)
                                    Rectangle {
                                        objectName: graphButton.objectName + "_accent"
                                        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                        width: 3; color: graphButton.palette.accent; visible: graphButton.emphasised
                                    }
                                    Rectangle {
                                        anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter
                                        width: 7; height: 7; radius: 4; visible: graphButton.marker !== ""
                                        color: graphButton.marker === "issue" ? Theme.errorText : graphButton.palette.accent
                                    }
                                }
                                Repeater {
                                    model: root.currentPage === graphEntry.modelData.key ? graphEditor.sections : []
                                    delegate: CategoryButton {
                                        required property string modelData
                                        objectName: "settingsNav_" + graphEntry.modelData.key + "_" + modelData
                                        text: GraphCatalog.groupNames[modelData]
                                        leftPadding: 22
                                        topPadding: 5; bottomPadding: 5
                                        highlighted: root.currentSection === modelData
                                        onClicked: root.jumpToSection(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---- Content ------------------------------------------------------
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage === "profile" ? 0 : 1

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
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                            radius: 5
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

                ColumnLayout {
                    id: graphsPage
                    objectName: "graphsSettingsPage"
                    spacing: 8

                    Item {
                        objectName: "graphEditorArea"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        GraphSettingsEditor {
                            id: graphEditor
                            anchors.fill: parent
                            settingsVm: root.settingsVm
                            graphKey: root.settingsVm.selectedGraph
                            visualSettings: root.visualSettings
                        }
                    }
                }
            }
        }
    }
}
