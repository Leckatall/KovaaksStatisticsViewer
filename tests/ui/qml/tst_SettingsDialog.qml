import QtQuick
import QtTest
import QtQuick.Dialogs
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "SettingsDialogTest"
    when: windowShown
    width: 700
    height: 500
    visible: true

    Component {
        id: settingsDialogComponent
        SettingsDialog {}
    }

    function makeFakeSettingsVm(overrides) {
        return TestDoubles.makeFakeSettingsVm(Object.assign({
            kovaaksDir: "file:///C:/Kovaaks",
            profilePath: "file:///C:/Profile/profile.pb",
            profileLoaded: false,
            allSeriesConfigs: [
                {id: "1", name: "Score", color: "#009600", enabled: true, displayPosition: 0},
                {id: "2", name: "Accuracy", color: "#00ffff", enabled: false, displayPosition: 1}
            ]
        }, overrides))
    }

    // A sessionVm whose properties notify, for tests that change them while the dialog is open.
    Component {
        id: observableSessionVmComponent
        QtObject {
            property bool profileBuildInProgress: false
            property double profileBuildProgress: 0
            property int profileBuildDone: 0
            property int profileBuildTotal: 0
            function generateProfile() {}
        }
    }

    function makeFakeSessionVm(overrides) {
        return Object.assign({
            generateProfileCalls: 0,
            generateProfile: function () { this.generateProfileCalls++ },
            profileBuildInProgress: false,
            profileBuildProgress: 0,
            profileBuildDone: 0,
            profileBuildTotal: 0
        }, overrides)
    }

    // Opens the dialog and waits for its Popup content (created on open,
    // not at construction) to actually show up in the item tree.
    //
    property var visualSettings: QtObject {
        property string graphAxisSeriesId: ""
    }

    function openDialog(props): SettingsDialog {
        const fullProps = Object.assign({visualSettings: testCase.visualSettings}, props)
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, fullProps)
        verify(dialog !== null, "SettingsDialog failed to instantiate")
        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after open()")
        return dialog
    }

    function selectCategory(dialog, categoryName) {
        const item = ItemLookup.findByObjectName(dialog.contentItem, "categoryItem_" + categoryName)
        verify(item !== null, "no category item named 'categoryItem_" + categoryName + "' found in SettingsDialog")
        mouseClick(item)
        // StackLayout doesn't flip the new page's `visible` synchronously
        // with the click, so give it a render pass before inspecting it.
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never re-rendered after selecting category '" + categoryName + "'")
    }

    // Opens at 860x620 (bounded only by the room the main window or screen gives it), never
    // below the old 680x460, and reopens at the size it was left at.
    function test_opensLargeAndRemembersItsSize() {
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm()})
        const limitWidth = dialog._limitWidth()
        const limitHeight = dialog._limitHeight()
        compare(dialog.width, Math.min(860, limitWidth))
        compare(dialog.height, Math.min(620, limitHeight))
        verify(dialog.width >= Math.min(680, limitWidth) && dialog.height >= Math.min(460, limitHeight),
               "never smaller than the old 680x460 while there is room")

        dialog.width = Math.min(700, limitWidth)
        dialog.height = Math.min(500, limitHeight)
        dialog.close()
        dialog.open()
        compare(dialog.width, Math.min(700, limitWidth), "reopens at the width it was left at")
        compare(dialog.height, Math.min(500, limitHeight))

        dialog.width = 500
        dialog.close()
        dialog.open()
        compare(dialog.width, Math.min(680, limitWidth), "a remembered size below the floor opens at the floor")

        // Leave the default for the other tests in this process.
        dialog.width = 860
        dialog.height = 620
        dialog.close()
    }

    function test_kovaaksDirField_showsSettingsVmDirWithoutFileScheme() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({kovaaksDir: "file:///D:/CustomDir"}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const field = ItemLookup.findByObjectName(dialog.contentItem, "kovaaksDirField")
        verify(field !== null, "no field named 'kovaaksDirField' found in SettingsDialog")
        compare(field.text, "D:/CustomDir", "kovaaksDirField should show settingsVm.kovaaksDir with the file:// scheme stripped")
    }

    // a path longer than its field must neither lose its start nor its
    // end to horizontal scrolling; it wraps so the whole path (including the
    // final folder / file name) is on screen without editing the field.
    function test_longPathsWrapSoTheWholePathIsVisible() {
        const longDir = "C:/Users/Student/CLionProjects/KovaaksStatisticsViewer/.temp/ui-driver/default/some/deeply/nested/folder/data"
        const longProfile = longDir + "/profile.pb"
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({kovaaksDir: "file:///" + longDir, profilePath: "file:///" + longProfile}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const dirField = ItemLookup.findByObjectName(dialog.contentItem, "kovaaksDirField")
        const profileField = ItemLookup.findByObjectName(dialog.contentItem, "profilePathField")
        verify(dirField !== null && profileField !== null, "both path fields must exist")

        compare(dirField.text, longDir, "the field must hold the full path")
        compare(profileField.text, longProfile, "the field must hold the full path")
        for (const field of [dirField, profileField]) {
            verify(field.lineCount > 1, "a path longer than the field must wrap onto further lines")
            verify(field.contentWidth <= field.width,
                   "the path must not overflow the field horizontally (contentWidth " + field.contentWidth
                   + " > width " + field.width + ")")
            verify(field.height >= field.contentHeight,
                   "every wrapped line must fit inside the field (height " + field.height
                   + " < contentHeight " + field.contentHeight + ")")
            compare(field.readOnly, true, "a path field is not editable")
        }
    }

    function test_directoriesCategory_showsProfilePathAndLoadedState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profilePath: "file:///D:/CustomProfile/profile.pb", profileLoaded: true}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const field = ItemLookup.findByObjectName(dialog.contentItem, "profilePathField")
        verify(field !== null, "no field named 'profilePathField' found in SettingsDialog Directories category")
        compare(field.text, "D:/CustomProfile/profile.pb", "profilePathField should show settingsVm.profilePath with the file:// scheme stripped")

        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Directories category")
        compare(statusLabel.text, "Profile status: Loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === true")
    }

    function test_directoriesCategory_showsNotLoadedWhenProfileVmReportsFalse() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: false}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Profile category")
        compare(statusLabel.text, "Profile status: Not loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === false")
    }

    function test_generateProfileButton_delegatesToSessionVm() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const button = ItemLookup.findByObjectName(dialog.contentItem, "generateProfileButton")
        verify(button !== null, "no button named 'generateProfileButton' found in SettingsDialog Profile category")
        mouseClick(button)

        compare(dialog.sessionVm.generateProfileCalls, 1, "clicking generateProfileButton should call sessionVm.generateProfile() once")
    }

    // the label matches the app's Title Case buttons and spells
    // "Kovaaks" and "Directory" the way the rest of the UI does.
    function test_generateProfileButton_labelIsClearAndConsistentlyCapitalised() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const button = ItemLookup.findByObjectName(dialog.contentItem, "generateProfileButton")
        verify(button !== null, "no button named 'generateProfileButton' found in SettingsDialog Profile category")
        compare(button.text, "Rebuild Profile from Kovaaks Directory")
        verify(!/kovaaks/.test(button.text), "'Kovaaks' must be capitalised like the rest of the UI")
        verify(!/\bdir\b/i.test(button.text), "the abbreviation 'dir' must not be used")
    }

    function test_profileBuildProgressBar_hiddenWhenNoBuildIsRunning() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const bar = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        verify(bar !== null, "no progress bar named 'profileBuildProgressBar' found in SettingsDialog Profile category")
        compare(bar.visible, false, "the progress bar should be hidden while no build is running")
    }

    function test_profileBuildProgressBar_showsProgressWhileBuilding() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true, profileBuildProgress: 0.4}),
            visualSettings: visualSettings
        })

        const bar = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        verify(bar !== null, "no progress bar named 'profileBuildProgressBar' found in SettingsDialog Profile category")
        compare(bar.visible, true, "the progress bar should be visible while a build is running")
        compare(bar.value, 0.4, "the progress bar should bind to sessionVm.profileBuildProgress")
        compare(bar.indeterminate, false, "a known fraction should not render as indeterminate")
    }

    // The file count is only known once the first per-file report lands.
    function test_profileBuildProgressBar_indeterminateBeforeTheFirstReport() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true}),
            visualSettings: visualSettings
        })

        const bar = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        compare(bar.indeterminate, true, "a build with no progress yet should render as indeterminate")
    }

    function test_profileStatus_saysRebuildingWhileABuildRuns_andRestoresAfter() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: true}),
            sessionVm: createTemporaryObject(observableSessionVmComponent, testCase, {profileBuildInProgress: true}),
            visualSettings: visualSettings
        })
        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        compare(statusLabel.text, "Profile status: Rebuilding", "a running rebuild replaces the loaded state in the status line")

        dialog.sessionVm.profileBuildInProgress = false
        compare(statusLabel.text, "Profile status: Loaded", "the status line shows the real state again once the rebuild ends")
    }

    function test_profileStatus_notLoadedAfterAFailedRebuild() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: false}),
            sessionVm: createTemporaryObject(observableSessionVmComponent, testCase, {profileBuildInProgress: true}),
            visualSettings: visualSettings
        })
        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        compare(statusLabel.text, "Profile status: Rebuilding")

        dialog.sessionVm.profileBuildInProgress = false
        compare(statusLabel.text, "Profile status: Not loaded")
    }

    function test_profileBuildProgressLabel_hiddenWhenNoBuildIsRunning() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })
        const label = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressLabel")
        verify(label !== null, "no label named 'profileBuildProgressLabel' found in SettingsDialog Profile category")
        compare(label.visible, false)
    }

    function test_profileBuildProgressLabel_saysFilesDoneOutOfTotal() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true, profileBuildProgress: 0.4,
                                          profileBuildDone: 2, profileBuildTotal: 5}),
            visualSettings: visualSettings
        })
        const label = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressLabel")
        compare(label.visible, true)
        compare(label.text, "Rebuilding profile: 2 of 5 files")
    }

    function test_profileBuildProgressLabel_saysScanningBeforeTheFirstReport() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true}),
            visualSettings: visualSettings
        })
        const label = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressLabel")
        compare(label.visible, true)
        compare(label.text, "Scanning Kovaaks Directory...")
    }

    function test_seriesEnabledSwitchesListAllSeriesConfigsAndReflectEnabledState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        selectCategory(dialog, "Graph Lines")

        const scoreSwitch = ItemLookup.findByObjectName(dialog.contentItem, "seriesEnabledSwitch_1")
        const accuracySwitch = ItemLookup.findByObjectName(dialog.contentItem, "seriesEnabledSwitch_2")
        verify(scoreSwitch !== null)
        verify(accuracySwitch !== null)
        compare(scoreSwitch.checked, true)
        compare(accuracySwitch.checked, false)
    }

    function test_manySeriesRowsSaveButtonStaysWithinTheSettingsWindowWhenScrolling() {
        const rows = []
        for (let i = 0; i < 20; i++)
            rows.push({id: String(i), name: "Series " + i, color: "#009600", enabled: true, displayPosition: i})
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({allSeriesConfigs: rows, pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })
        selectCategory(dialog, "Graph Lines")

        const saveButton = ItemLookup.findByObjectName(dialog.contentItem, "saveGraphLinesButton")
        verify(saveButton !== null, "no item named 'saveGraphLinesButton' found in SettingsDialog")
        const point = saveButton.mapToItem(dialog.contentItem, 0, 0)
        verify(point.y >= 0 && point.y + saveButton.height <= dialog.contentItem.height, "saveGraphLinesButton should remain visible within the SettingsDialog window when many series are present")
    }

    function test_categoryList_startsOnDirectories() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        compare(dialog.currentCategory, 0, "SettingsDialog should default to the Directories category (index 0)")
    }

    function test_clickingCategoryItem_switchesCurrentCategory() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        selectCategory(dialog, "Graph Lines")

        compare(dialog.currentCategory, 1, "clicking the 'Graph Lines' category item should switch currentCategory to index 1")
    }

    function test_openGraphLinesOpensDialogOnGraphLinesCategory() {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        dialog.openGraphLines()

        tryCompare(dialog, "visible", true)
        compare(dialog.currentCategory, dialog.graphLinesCategory)
    }

    function test_closingWithNoPendingChangesClosesImmediatelyWithoutPrompt() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: false}),
            sessionVm: makeFakeSessionVm(),
        })

        dialog.close()

        tryCompare(dialog, "visible", false)
        compare(dialog.settingsVm.discardDraftCalls, 0)
    }

    function test_closingWithPendingChangesShowsConfirmPromptAndStaysOpenUntilResolved() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
        })

        dialog.close()

        tryCompare(dialog, "visible", true)
        tryCompare(dialog.discardChangesPrompt, "visible", true)
    }

    function test_confirmingDiscardOnCloseCallsDiscardSeriesDraftThenCloses() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)

        tryCompare(dialog.settingsVm, "discardDraftCalls", 1)
        tryCompare(dialog, "visible", false)
    }

    function test_savingOnCloseCallsCommitSeriesDraftThenCloses() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(dialog.settingsVm, "commitDraftCalls", 1)
        tryCompare(dialog, "visible", false)
    }

    function test_cancellingCloseLeavesWindowOpenAndDraftUntouched() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)

        compare(dialog.visible, true)
        compare(dialog.settingsVm.commitDraftCalls, 0)
        compare(dialog.settingsVm.discardDraftCalls, 0)
    }

    function test_switchingCategoriesDoesNotRetriggerDraftLifecycle() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        selectCategory(dialog, "Graph Lines")
        selectCategory(dialog, "Directories")
        selectCategory(dialog, "Graph Lines")

        compare(dialog.settingsVm.discardDraftCalls, 0)
    }

    function test_openingDialogCallsBeginSeriesDraft() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        const panel = ItemLookup.findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
        verify(panel !== null, "no item named 'seriesConfigDraftPanel' found in SettingsDialog")
        compare(dialog.settingsVm.beginDraftCalls, 1, "beginSeriesDraft should be called when the dialog is opened")
    }

    function test_reopeningDialogAfterCloseCallsBeginSeriesDraftAgain() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: false}),
            sessionVm: makeFakeSessionVm(),
        })

        const panel = ItemLookup.findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
        verify(panel !== null, "no item named 'seriesConfigDraftPanel' found in SettingsDialog")
        compare(dialog.settingsVm.beginDraftCalls, 1, "beginSeriesDraft should have fired on the first open()")

        dialog.close()
        tryCompare(dialog, "visible", false)

        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after reopening")

        compare(dialog.settingsVm.beginDraftCalls, 2, "beginSeriesDraft must be called again every time the dialog is opened, not just once at construction")
    }

    function test_reopeningAfterSavingPendingChangesCallsBeginSeriesDraftAgain() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm(),
        })

        const panel = ItemLookup.findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
        verify(panel !== null, "no item named 'seriesConfigDraftPanel' found in SettingsDialog")
        compare(dialog.settingsVm.beginDraftCalls, 1, "beginSeriesDraft should have fired on the first open()")

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(dialog.settingsVm, "commitDraftCalls", 1)
        tryCompare(dialog, "visible", false)

        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after reopening")

        compare(dialog.settingsVm.beginDraftCalls, 2, "beginSeriesDraft must be called again on reopen after a save, matching the reported bug: Save-and-reopen must start a fresh draft")
    }

}
