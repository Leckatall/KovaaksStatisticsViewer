import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    objectName: "benchmarkManagerDialog"
    title: qsTr("Benchmark Manager")
    flags: Qt.Dialog
    modality: Qt.WindowModal
    width: 1040
    height: 700
    minimumWidth: 880
    minimumHeight: 560

    required property var benchmarkManagerVm

    readonly property alias dirtyClosePrompt: dirtyClosePrompt
    readonly property alias retrospectivePrompt: retrospectivePrompt
    readonly property alias deleteConfirmPrompt: deleteConfirmPrompt

    property string selectedEntryFilename: ""
    property string focusTargetId: ""
    property string statusMessage: ""
    property bool statusIsError: false
    property string importStatus: ""
    property var pendingAction: null

    // A mapping edit or a save on an opened (from-library) definition must clear the
    // retrospective reinterpretation warning first; the confirmed action is parked here.
    property var pendingRetrospectiveAction: null
    property string knownScenarioFilter: ""
    property string selectedKnownScenarioHash: ""
    property string selectedKnownScenarioName: ""

    function selectedRow() {
        const entries = root.benchmarkManagerVm.libraryEntries
        if (!root.selectedEntryFilename || !entries) return null
        return entries.find(entry => entry.filename === root.selectedEntryFilename) || null
    }

    function open() {
        root.selectedEntryFilename = ""
        root.focusTargetId = ""
        root.statusMessage = ""
        root.statusIsError = false
        root.importStatus = ""
        root.pendingAction = null
        visible = true
        raise()
        requestActivate()
    }

    onClosing: (close) => {
        if (root.benchmarkManagerVm.dirty) {
            close.accepted = false
            root.runWhenClean(() => root.visible = false)
        }
    }

    // Every draft-replacing transition (New / Open / Import / close) waits for an explicit
    // Save/Discard/Cancel decision while a dirty draft exists, so unfinished work can never
    // be silently dropped.
    function runWhenClean(action) {
        if (!root.benchmarkManagerVm.dirty) {
            action()
            return
        }
        root.pendingAction = action
        dirtyClosePrompt.open()
    }

    function runPendingAction() {
        const action = root.pendingAction
        root.pendingAction = null
        if (action) action()
    }

    // A deferred action belongs to the transition that armed it: a save that fails abandons that
    // transition, so the action must be dropped rather than left to fire on a later save.
    function doSave() {
        const result = root.benchmarkManagerVm.save()
        root.statusMessage = result.ok ? qsTr("Saved.") : result.error
        root.statusIsError = !result.ok
        if (result.ok) runPendingAction()
        else root.pendingAction = null
        return result.ok
    }

    function requestSave() {
        root.runRetrospective(() => root.doSave())
    }

    // Edits to a saved definition reinterpret past results, so they pass through the
    // retrospective warning; a never-saved draft applies the action immediately.
    function runRetrospective(action) {
        if (root.benchmarkManagerVm.draftFromLibrary) {
            root.pendingRetrospectiveAction = action
            retrospectivePrompt.open()
        } else {
            action()
        }
    }

    function requestMappingEdit(entryId, hash) {
        root.runRetrospective(() => root.benchmarkManagerVm.setScenarioHash(entryId, hash))
    }

    function matchStateLabel(state) {
        return ({
            resolved: qsTr("Resolved"),
            mappedUnavailable: qsTr("Mapped, unavailable"),
            unresolved: qsTr("Unresolved"),
            ambiguous: qsTr("Ambiguous"),
            autoMappable: qsTr("Pending automatic mapping")
        })[state] || qsTr("Unresolved")
    }

    function offersMappingEdit(state) {
        return state === "resolved" || state === "mappedUnavailable"
    }

    function candidateText(candidate) {
        const parts = [String(candidate.hash),
                       qsTr("%n run(s)", "", candidate.runCount || 0)]
        const played = candidate.lastPlayed
        if (played && !isNaN(new Date(played).getTime()))
            parts.push(Qt.formatDate(played, "yyyy-MM-dd"))
        return parts.join(" · ")
    }

    function filteredCatalogue() {
        const query = root.knownScenarioFilter.trim().toLowerCase()
        if (query === "") return []
        const catalogue = root.benchmarkManagerVm.scenarioCatalogue || []
        return catalogue.filter(entry => entry.name.toLowerCase().indexOf(query) !== -1)
    }

    function addSelectedKnownScenario() {
        if (root.selectedKnownScenarioHash === "") return
        root.benchmarkManagerVm.addKnownScenario(root.selectedKnownScenarioName,
                                                 root.selectedKnownScenarioHash)
        root.knownScenarioFilter = ""
        root.selectedKnownScenarioHash = ""
        root.selectedKnownScenarioName = ""
    }

    function importFromUrl(url) {
        root.runWhenClean(() => {
            const result = root.benchmarkManagerVm.importPlaylist(url)
            root.importStatus = result.ok
                ? (result.skipped.length > 0
                    ? qsTr("Imported — skipped %n duplicate(s).", "", result.skipped.length)
                    : qsTr("Imported."))
                : result.error
        })
    }

    function childScenarios(node) {
        return node.children.filter(child => child.kind === "scenario")
    }

    function childGroups(node) {
        return node.children.filter(child => child.kind !== "scenario")
    }

    function classificationColor(classification) {
        return ({
            Trackable: "#4CAF50",
            Incomplete: "#FFA726",
            Invalid: "#E53935",
            Unsupported: "#9E9E9E"
        })[classification] || "#9E9E9E"
    }

    function focusColor(nodeId) {
        return root.focusTargetId !== "" && root.focusTargetId === nodeId
            ? Qt.alpha(root.palette.accent, 0.25)
            : "transparent"
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
    }

    MessageDialog {
        id: dirtyClosePrompt
        objectName: "dirtyClosePrompt"
        title: qsTr("Unsaved changes")
        text: qsTr("You have unsaved changes in the benchmark editor.")
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: function (button) {
            if (button === MessageDialog.Save) {
                root.requestSave()
            } else if (button === MessageDialog.Discard) {
                root.benchmarkManagerVm.discard()
                root.runPendingAction()
            } else {
                root.pendingAction = null
            }
        }
    }

    MessageDialog {
        id: retrospectivePrompt
        objectName: "retrospectivePrompt"
        title: qsTr("Recalculate history?")
        text: qsTr("Saving these changes reinterprets past results under the edited definition.")
        informativeText: qsTr("Thresholds, membership, tier order, and hierarchy all feed the historical graphs.")
        buttons: MessageDialog.Save | MessageDialog.Cancel
        onButtonClicked: function (button) {
            const action = root.pendingRetrospectiveAction
            root.pendingRetrospectiveAction = null
            if (button === MessageDialog.Save) {
                if (action) action()
            } else {
                root.pendingAction = null
            }
        }
    }

    MessageDialog {
        id: deleteConfirmPrompt
        objectName: "deleteConfirmPrompt"
        title: qsTr("Delete benchmark?")
        buttons: MessageDialog.Yes | MessageDialog.Cancel
        property string targetId
        property string targetName
        text: qsTr("Delete \"%1\"? Its file will be removed from the managed directory.").arg(targetName)
        onButtonClicked: function (button) {
            if (button !== MessageDialog.Yes) return
            const remove = () => {
                const result = root.benchmarkManagerVm.deleteBenchmark(deleteConfirmPrompt.targetId)
                root.statusMessage = result.ok ? qsTr("Deleted.") : result.error
                root.statusIsError = !result.ok
                if (result.ok) root.selectedEntryFilename = ""
            }
            // Deleting the benchmark currently being edited discards the draft with it, so it is a
            // draft-replacing transition too; deleting any other entry leaves the draft alone.
            if (deleteConfirmPrompt.targetId === root.benchmarkManagerVm.draftId) root.runWhenClean(remove)
            else remove()
        }
    }

    FileDialog {
        id: importDialog
        objectName: "importPlaylistDialog"
        title: qsTr("Import playlist")
        fileMode: FileDialog.OpenFile
        nameFilters: ["KovaaKs Playlist (*.json)", "All files (*)"]
        onAccepted: root.importFromUrl(importDialog.selectedFiles[0])
    }

    ColorDialog {
        id: colorDialog
        objectName: "colorDialog"
        property string targetKind
        property string targetId
        onAccepted: {
            if (colorDialog.targetKind === "tier")
                root.benchmarkManagerVm.setTierColor(colorDialog.targetId, colorDialog.selectedColor)
            else
                root.benchmarkManagerVm.setGroupColor(colorDialog.targetId, colorDialog.selectedColor)
        }
    }

    Menu {
        id: moveMenu
        property string scenarioId

        function openFor(nodeId) {
            moveMenu.scenarioId = nodeId
            moveMenu.popup()
        }

        Instantiator {
            model: root.flattenGroupTargets()
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                onTriggered: root.benchmarkManagerVm.moveScenario(moveMenu.scenarioId, modelData.id)
            }
            onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => moveMenu.removeItem(object)
        }
    }

    function flattenGroupTargets() {
        const targets = [{id: "", label: qsTr("Uncategorized")}]
        const treeRoot = root.benchmarkManagerVm.root
        if (!treeRoot) return targets
        for (const category of root.childGroups(treeRoot)) {
            targets.push({id: category.nodeId, label: category.name})
            for (const sub of root.childGroups(category))
                targets.push({id: sub.nodeId, label: category.name + " / " + sub.name})
        }
        return targets
    }

    // ---- Shared tree pieces --------------------------------------------------

    component ColorSwatchButton: Button {
        id: colorSwatchButton
        property string swatchKind
        property string swatchId
        property color swatchColor
        background: Rectangle {
            color: colorSwatchButton.swatchColor; radius: 4
        }
        onClicked: {
            colorDialog.targetKind = colorSwatchButton.swatchKind
            colorDialog.targetId = colorSwatchButton.swatchId
            colorDialog.open()
        }
    }

    component ScenarioRow: Rectangle {
        id: scenarioRow
        required property var modelData
        // Candidates for an ambiguous entry are shown up front; for a resolved or
        // mapped-unavailable entry they stay hidden until "Change mapping" is pressed.
        property bool mappingCandidatesShown: scenarioRow.modelData.matchState === "ambiguous"
        objectName: "treeNode_" + scenarioRow.modelData.nodeId
        color: root.focusColor(scenarioRow.modelData.nodeId)
        radius: 4
        implicitHeight: scenarioBody.implicitHeight + 8
        Layout.fillWidth: true

        ColumnLayout {
            id: scenarioBody
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            RowLayout {
                id: scenarioLayout
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    Layout.preferredWidth: 170
                    text: scenarioRow.modelData.name
                    selectByMouse: true
                    onEditingFinished:
                        root.benchmarkManagerVm.renameScenario(scenarioRow.modelData.nodeId, text)
                }
                Repeater {
                    model: scenarioRow.modelData.thresholds
                    delegate: RowLayout {
                        id: thresholdRow
                        required property var modelData
                        spacing: 4
                        Label {
                            text: thresholdRow.modelData.tierName
                            Layout.alignment: Qt.AlignVCenter
                        }
                        TextField {
                            objectName: "thresholdField_" + scenarioRow.modelData.nodeId + "_" + thresholdRow.modelData.tierId
                            Layout.preferredWidth: 72
                            placeholderText: "—"
                            text: thresholdRow.modelData.hasValue ? thresholdRow.modelData.score : ""
                            validator: DoubleValidator {
                                bottom: 0; decimals: 6
                            }
                            onEditingFinished: {
                                if (text.trim() === "") {
                                    root.benchmarkManagerVm.clearThreshold(
                                        scenarioRow.modelData.nodeId, thresholdRow.modelData.tierId)
                                } else if (!isNaN(parseFloat(text))) {
                                    root.benchmarkManagerVm.setThreshold(
                                        scenarioRow.modelData.nodeId, thresholdRow.modelData.tierId,
                                        parseFloat(text))
                                }
                            }
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    objectName: "moveScenarioButton_" + scenarioRow.modelData.nodeId
                    text: qsTr("Move...")
                    onClicked: moveMenu.openFor(scenarioRow.modelData.nodeId)
                }
                Button {
                    objectName: "removeScenario_" + scenarioRow.modelData.nodeId
                    text: qsTr("Remove")
                    onClicked: root.benchmarkManagerVm.removeScenario(scenarioRow.modelData.nodeId)
                }
            }

            RowLayout {
                id: resolutionLayout
                Layout.fillWidth: true
                spacing: 8

                Label {
                    objectName: "matchLabel_" + scenarioRow.modelData.nodeId
                    text: root.matchStateLabel(scenarioRow.modelData.matchState)
                    color: scenarioRow.palette.placeholderText
                }
                Button {
                    objectName: "changeMappingButton_" + scenarioRow.modelData.nodeId
                    text: qsTr("Change mapping")
                    visible: root.offersMappingEdit(scenarioRow.modelData.matchState)
                    Accessible.name: qsTr("Change the scenario mapping for %1").arg(scenarioRow.modelData.name)
                    onClicked: scenarioRow.mappingCandidatesShown = !scenarioRow.mappingCandidatesShown
                }
                Button {
                    objectName: "clearMappingButton_" + scenarioRow.modelData.nodeId
                    text: qsTr("Clear mapping")
                    visible: root.offersMappingEdit(scenarioRow.modelData.matchState)
                    Accessible.name: qsTr("Clear the scenario mapping for %1").arg(scenarioRow.modelData.name)
                    onClicked: root.requestMappingEdit(scenarioRow.modelData.nodeId, "")
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            // The candidate rows stay in the layout whenever the entry has any, so their
            // geometry is settled at first render; "Change mapping" only arms them for
            // selection (ambiguous entries are armed from the start). Keeping the size stable
            // means a test that clicks a candidate straight after arming it still lands on it.
            Item {
                id: candidateArea
                Layout.fillWidth: true
                Layout.leftMargin: 24
                clip: true
                Layout.preferredHeight: candidateRepeater.count * 40

                Repeater {
                    id: candidateRepeater
                    model: scenarioRow.modelData.candidates || []
                    delegate: ItemDelegate {
                        id: candidateOption
                        required property var modelData
                        required property int index
                        objectName: "candidateOption_" + scenarioRow.modelData.nodeId + "_" + candidateOption.modelData.hash
                        y: candidateOption.index * 40
                        width: candidateArea.width
                        height: 40
                        enabled: scenarioRow.mappingCandidatesShown
                        opacity: scenarioRow.mappingCandidatesShown ? 1 : 0.4
                        text: root.candidateText(candidateOption.modelData)
                        Accessible.name: qsTr("Map %1 to %2").arg(scenarioRow.modelData.name).arg(candidateOption.text)
                        onClicked: root.requestMappingEdit(scenarioRow.modelData.nodeId, candidateOption.modelData.hash)
                    }
                }
            }
        }
    }

    // Subcategories hold scenarios only; their shape is fixed one level below categories.
    component SubcategoryCard: ColumnLayout {
        id: subcategoryCard
        required property var modelData
        spacing: 4
        Layout.fillWidth: true

        Rectangle {
            objectName: "treeNode_" + subcategoryCard.modelData.nodeId
            color: root.focusColor(subcategoryCard.modelData.nodeId)
            radius: 4
            implicitHeight: subHeader.implicitHeight + 8
            Layout.fillWidth: true

            RowLayout {
                id: subHeader
                anchors.fill: parent
                anchors.margins: 4
                spacing: 8

                ColorSwatchButton {
                    swatchKind: "group"
                    swatchId: subcategoryCard.modelData.nodeId
                    swatchColor: subcategoryCard.modelData.color
                }
                Label {
                    text: qsTr("Subcategory"); color: subcategoryCard.palette.placeholderText
                }
                TextField {
                    Layout.fillWidth: true
                    text: subcategoryCard.modelData.name
                    selectByMouse: true
                    onEditingFinished:
                        root.benchmarkManagerVm.renameGroup(subcategoryCard.modelData.nodeId, text)
                }
                Button {
                    objectName: "removeGroup_" + subcategoryCard.modelData.nodeId
                    text: qsTr("Remove")
                    onClicked: root.benchmarkManagerVm.removeCategory(subcategoryCard.modelData.nodeId)
                }
            }
        }

        Repeater {
            model: root.childScenarios(subcategoryCard.modelData)
            delegate: ScenarioRow {
            }
        }
    }

    component CategoryCard: ColumnLayout {
        id: categoryCard
        required property var modelData
        required property int index
        spacing: 4
        Layout.fillWidth: true

        Rectangle {
            objectName: "treeNode_" + categoryCard.modelData.nodeId
            color: root.focusColor(categoryCard.modelData.nodeId)
            radius: 4
            implicitHeight: categoryHeader.implicitHeight + 8
            Layout.fillWidth: true

            RowLayout {
                id: categoryHeader
                anchors.fill: parent
                anchors.margins: 4
                spacing: 8

                ColorSwatchButton {
                    swatchKind: "group"
                    swatchId: categoryCard.modelData.nodeId
                    swatchColor: categoryCard.modelData.color
                }
                Label {
                    text: qsTr("Category"); color: categoryCard.palette.placeholderText
                }
                TextField {
                    Layout.fillWidth: true
                    text: categoryCard.modelData.name
                    selectByMouse: true
                    onEditingFinished:
                        root.benchmarkManagerVm.renameGroup(categoryCard.modelData.nodeId, text)
                }
                Button {
                    objectName: "moveCategoryUp_" + categoryCard.modelData.nodeId
                    text: qsTr("↑")
                    enabled: categoryCard.index > 0
                    onClicked:
                        root.benchmarkManagerVm.reorderCategory(categoryCard.modelData.nodeId,
                            categoryCard.index - 1)
                }
                Button {
                    objectName: "moveCategoryDown_" + categoryCard.modelData.nodeId
                    text: qsTr("↓")
                    enabled: root.benchmarkManagerVm.root
                        && categoryCard.index < root.childGroups(root.benchmarkManagerVm.root).length - 1
                    onClicked:
                        root.benchmarkManagerVm.reorderCategory(categoryCard.modelData.nodeId,
                            categoryCard.index + 1)
                }
                TextField {
                    id: newSubcategoryField
                    objectName: "newSubcategoryField_" + categoryCard.modelData.nodeId
                    Layout.preferredWidth: 150
                    placeholderText: qsTr("Subcategory name")
                    selectByMouse: true
                }
                Button {
                    objectName: "addSubcategory_" + categoryCard.modelData.nodeId
                    text: qsTr("Add Subcategory")
                    enabled: newSubcategoryField.text.trim() !== ""
                    onClicked: {
                        root.benchmarkManagerVm.addSubcategory(categoryCard.modelData.nodeId,
                            newSubcategoryField.text)
                        newSubcategoryField.clear()
                    }
                }
                Button {
                    objectName: "removeGroup_" + categoryCard.modelData.nodeId
                    text: qsTr("Remove")
                    onClicked: root.benchmarkManagerVm.removeCategory(categoryCard.modelData.nodeId)
                }
            }
        }

        Repeater {
            model: root.childScenarios(categoryCard.modelData)
            delegate: ScenarioRow {
            }
        }
        Repeater {
            model: root.childGroups(categoryCard.modelData)
            delegate: SubcategoryCard {
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ---- Library pane ------------------------------------------------
            ColumnLayout {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                spacing: 8

                Label {
                    text: qsTr("Library")
                    font.pixelSize: 18
                    font.bold: true
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Button {
                        objectName: "newBenchmarkButton"
                        text: qsTr("New")
                        onClicked: root.runWhenClean(() => root.benchmarkManagerVm.beginNewBenchmark())
                    }
                    Button {
                        objectName: "importPlaylistButton"
                        text: qsTr("Import playlist...")
                        onClicked: importDialog.open()
                    }
                    Button {
                        objectName: "openBenchmarkButton"
                        text: qsTr("Open")
                        enabled: root.selectedRow() !== null && root.selectedRow().openable
                        onClicked: {
                            const row = root.selectedRow()
                            root.runWhenClean(() => root.benchmarkManagerVm.openBenchmark(row.id))
                        }
                    }
                    Button {
                        objectName: "deleteBenchmarkButton"
                        text: qsTr("Delete")
                        enabled: root.selectedRow() !== null && root.selectedRow().deletable
                        onClicked: {
                            deleteConfirmPrompt.targetId = root.selectedRow().id
                            deleteConfirmPrompt.targetName = root.selectedRow().name
                            deleteConfirmPrompt.open()
                        }
                    }
                    Button {
                        objectName: "refreshLibraryButton"
                        text: qsTr("Refresh")
                        enabled: !root.benchmarkManagerVm.dirty
                        onClicked: root.benchmarkManagerVm.refresh()
                    }
                    Button {
                        objectName: "openDirectoryButton"
                        text: qsTr("Open directory")
                        onClicked:
                            Qt.openUrlExternally(encodeURI("file:///" + root.benchmarkManagerVm.managedDirectoryPath))
                    }
                }

                Label {
                    objectName: "importStatusLabel"
                    visible: root.importStatus !== ""
                    text: root.importStatus
                    color: root.importStatus.toLowerCase().indexOf("imported") === 0
                        ? root.palette.windowText : "#E57373"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    objectName: "refreshFailedLabel"
                    visible: root.benchmarkManagerVm.refreshFailed
                    text: qsTr("Last refresh failed: the directory could not be read.")
                    color: "#E57373"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    objectName: "saveStatusLabel"
                    visible: root.statusMessage !== ""
                    text: root.statusMessage
                    color: root.statusIsError ? "#E57373" : root.palette.windowText
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    id: libraryList
                    objectName: "libraryList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.benchmarkManagerVm.libraryEntries
                    delegate: ItemDelegate {
                        id: libraryEntry
                        required property var modelData
                        required property int index
                        width: libraryList.width
                        objectName: "libraryEntry_" + index
                        highlighted: root.selectedEntryFilename === libraryEntry.modelData.filename
                        onClicked: root.selectedEntryFilename = libraryEntry.modelData.filename
                        contentItem: RowLayout {
                            spacing: 8
                            Label {
                                text: libraryEntry.modelData.name
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: root.classificationColor(libraryEntry.modelData.classification)
                            }
                            Label {
                                text: libraryEntry.modelData.classification
                                color: libraryEntry.palette.placeholderText
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: root.palette.mid
            }

            // ---- Editor pane ---------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                visible: root.benchmarkManagerVm.hasDraft

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: benchmarkNameField
                        objectName: "benchmarkNameField"
                        placeholderText: qsTr("Benchmark name")
                        Layout.fillWidth: true
                        selectByMouse: true
                        onEditingFinished: root.benchmarkManagerVm.setBenchmarkName(text)

                        // Typing into a TextField destroys a plain `text:` binding, and this pane is
                        // only hidden (never destroyed) when the draft closes, so the field would keep
                        // the old name across Discard/New. Binding re-asserts it whenever the field is
                        // not the one being edited.
                        Binding {
                            target: benchmarkNameField
                            property: "text"
                            value: root.benchmarkManagerVm.benchmarkName
                            when: !benchmarkNameField.activeFocus
                            restoreMode: Binding.RestoreNone
                        }
                    }
                    Label {
                        objectName: "draftStatusBadge"
                        text: root.benchmarkManagerVm.draftTrackable ? qsTr("Trackable") : qsTr("Incomplete")
                    }
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.classificationColor(root.benchmarkManagerVm.draftTrackable
                            ? "Trackable" : "Incomplete")
                    }
                    Button {
                        objectName: "saveButton"
                        text: qsTr("Save")
                        onClicked: root.requestSave()
                    }
                    Button {
                        objectName: "discardButton"
                        text: qsTr("Discard")
                        onClicked: {
                            root.benchmarkManagerVm.discard()
                            root.statusMessage = ""
                        }
                    }
                }

                // Non-modal: the editor stays fully usable while this is shown. It is bound
                // straight to the flag, so the next reconciliation that publishes without a
                // write failure clears it on its own.
                Rectangle {
                    objectName: "resolutionFailureBanner"
                    visible: root.benchmarkManagerVm.resolutionWriteFailed
                    Layout.fillWidth: true
                    implicitHeight: resolutionFailureText.implicitHeight + 12
                    radius: 4
                    color: "#5D4037"
                    Label {
                        id: resolutionFailureText
                        objectName: "resolutionFailureBannerLabel"
                        anchors.fill: parent
                        anchors.margins: 6
                        wrapMode: Text.WordWrap
                        color: "#FFFFFF"
                        text: qsTr("Automatic scenario resolution could not be written. Your mappings are unchanged; the manager retries on the next refresh.")
                    }
                }

                Label {
                    text: qsTr("Tiers")
                    font.pixelSize: 14
                    font.bold: true
                }

                ColumnLayout {
                    id: tiersEditor
                    objectName: "tiersEditor"
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: root.benchmarkManagerVm.tiers
                        delegate: RowLayout {
                            id: tierRow
                            required property var modelData
                            required property int index
                            spacing: 6

                            ColorSwatchButton {
                                objectName: "tierColor_" + tierRow.modelData.id
                                implicitWidth: 28
                                implicitHeight: 28
                                swatchKind: "tier"
                                swatchId: tierRow.modelData.id
                                swatchColor: tierRow.modelData.color
                            }
                            TextField {
                                objectName: "tierName_" + tierRow.modelData.id
                                Layout.preferredWidth: 120
                                text: tierRow.modelData.name
                                selectByMouse: true
                                onEditingFinished:
                                    root.benchmarkManagerVm.renameTier(tierRow.modelData.id, text)
                            }
                            Button {
                                text: qsTr("↑")
                                enabled: tierRow.index > 0
                                onClicked:
                                    root.benchmarkManagerVm.reorderTier(tierRow.modelData.id, tierRow.index - 1)
                            }
                            Button {
                                text: qsTr("↓")
                                enabled: tierRow.index < root.benchmarkManagerVm.tiers.length - 1
                                onClicked:
                                    root.benchmarkManagerVm.reorderTier(tierRow.modelData.id, tierRow.index + 1)
                            }
                            Button {
                                objectName: "removeTier_" + tierRow.modelData.id
                                text: qsTr("Remove")
                                onClicked: root.benchmarkManagerVm.removeTier(tierRow.modelData.id)
                            }
                        }
                    }

                    RowLayout {
                        spacing: 6
                        TextField {
                            id: newTierField
                            objectName: "newTierField"
                            placeholderText: qsTr("New tier name")
                            Layout.preferredWidth: 160
                        }
                        Button {
                            objectName: "addTierButton"
                            text: qsTr("Add Tier")
                            onClicked: {
                                if (newTierField.text.trim() === "") return
                                root.benchmarkManagerVm.addTier(newTierField.text)
                                newTierField.clear()
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("Validation")
                    font.pixelSize: 14
                    font.bold: true
                    visible: root.benchmarkManagerVm.validationIssues.length > 0
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ColumnLayout {
                        id: validationPanel
                        objectName: "validationPanel"
                        Layout.fillWidth: true
                        spacing: 2
                        visible: root.benchmarkManagerVm.validationIssues.length > 0

                        Repeater {
                            model: root.benchmarkManagerVm.validationIssues
                            delegate: Button {
                                id: validationIssue
                                required property var modelData
                                required property int index
                                objectName: "validationIssue_" + index
                                flat: true
                                Layout.fillWidth: true
                                contentItem: Text {
                                    text: "• " + validationIssue.modelData.message
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                    color: validationIssue.palette.buttonText
                                }
                                onClicked: root.focusTargetId = validationIssue.modelData.targetId
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("Scenarios")
                    font.pixelSize: 14
                    font.bold: true
                }

                ScrollView {
                    id: hierarchyScroll
                    objectName: "hierarchyScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        id: hierarchyColumn
                        spacing: 8
                        width: hierarchyScroll.availableWidth

                        RowLayout {
                            spacing: 6
                            TextField {
                                id: newScenarioField
                                objectName: "newScenarioField"
                                placeholderText: qsTr("Scenario name")
                                Layout.preferredWidth: 200
                            }
                            Button {
                                objectName: "addScenarioButton"
                                text: qsTr("Add Scenario")
                                onClicked: {
                                    if (newScenarioField.text.trim() === "") return
                                    root.benchmarkManagerVm.addUnplayedScenario(newScenarioField.text)
                                    newScenarioField.clear()
                                }
                            }
                            Label {
                                text: qsTr("Scenarios start in Uncategorized; use Move... to organize them.")
                                color: hierarchyColumn.palette.placeholderText
                                visible: root.benchmarkManagerVm.root
                                    && root.childScenarios(root.benchmarkManagerVm.root).length === 0
                                    && root.childGroups(root.benchmarkManagerVm.root).length === 0
                            }
                        }

                        // Profile-known scenarios are added by hash, so equal display names in the
                        // catalogue stay distinguishable; the free-text row above still adds an
                        // as-yet-unplayed scenario by name only.
                        RowLayout {
                            spacing: 6
                            TextField {
                                id: knownScenarioPicker
                                objectName: "knownScenarioPicker"
                                placeholderText: qsTr("Find a played scenario")
                                Layout.preferredWidth: 200
                                activeFocusOnTab: true
                                Accessible.name: qsTr("Find a played scenario to add by name")
                                onTextEdited: root.knownScenarioFilter = text
                                onEditingFinished: root.knownScenarioFilter = text
                            }
                            Button {
                                id: addKnownScenarioButton
                                objectName: "addKnownScenarioButton"
                                text: qsTr("Add Known Scenario")
                                activeFocusOnTab: true
                                Accessible.name: qsTr("Add the selected played scenario")
                                onClicked: root.addSelectedKnownScenario()
                            }
                        }

                        Repeater {
                            model: root.filteredCatalogue()
                            delegate: ItemDelegate {
                                id: knownScenarioOption
                                required property var modelData
                                objectName: "knownScenarioOption_" + knownScenarioOption.modelData.hash
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                highlighted: root.selectedKnownScenarioHash === knownScenarioOption.modelData.hash
                                text: knownScenarioOption.modelData.name + "  ·  " + knownScenarioOption.modelData.hash
                                Accessible.name: qsTr("Select played scenario %1").arg(knownScenarioOption.text)
                                onClicked: {
                                    root.selectedKnownScenarioHash = knownScenarioOption.modelData.hash
                                    root.selectedKnownScenarioName = knownScenarioOption.modelData.name
                                }
                            }
                        }

                        Repeater {
                            model: root.benchmarkManagerVm.root
                                ? root.childScenarios(root.benchmarkManagerVm.root) : []
                            delegate: ScenarioRow {
                            }
                        }
                        Repeater {
                            model: root.benchmarkManagerVm.root
                                ? root.childGroups(root.benchmarkManagerVm.root) : []
                            delegate: CategoryCard {
                            }
                        }
                    }
                }
            }

            Label {
                visible: !root.benchmarkManagerVm.hasDraft
                text: qsTr("Create, import, or open a benchmark to edit it.")
                color: root.palette.placeholderText
            }
        }
    }
}
