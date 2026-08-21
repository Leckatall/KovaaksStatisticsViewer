import QtQuick
import QtTest
import QtQuick.Dialogs
import "../../../src/ui/qml"

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
        return Object.assign({
            kovaaksDir: "file:///C:/Kovaaks",
            profilePath: "file:///C:/Profile/profile.pb",
            profileLoaded: false,
            allSeriesConfigs: [
                {id: "1", name: "Score", color: "#009600", enabled: true, displayPosition: 0},
                {id: "2", name: "Accuracy", color: "#00ffff", enabled: false, displayPosition: 1}
            ],
            setSeriesEnabledCalls: 0,
            lastSetSeriesEnabledId: null,
            lastSetSeriesEnabledValue: null,
            setSeriesEnabled: function (id, enabled) {
                this.setSeriesEnabledCalls++
                this.lastSetSeriesEnabledId = id
                this.lastSetSeriesEnabledValue = enabled
            },
            pendingChanges: false,
            beginDraftCalls: 0,
            beginSeriesDraft: function () { this.beginDraftCalls++ },
            commitDraftCalls: 0,
            commitSeriesDraft: function () { this.commitDraftCalls++; this.pendingChanges = false; return {succeeded: true} },
            discardSeriesDraftCalls: 0,
            discardSeriesDraft: function () { this.discardSeriesDraftCalls++; this.pendingChanges = false }
        }, overrides)
    }

    function makeFakeSessionVm(overrides) {
        return Object.assign({
            generateProfileCalls: 0,
            generateProfile: function () { this.generateProfileCalls++ },
            profileBuildInProgress: false,
            profileBuildProgress: 0
        }, overrides)
    }

    // TestCase.findChild doesn't reliably reach items nested under
    // StackLayout pages / ListView delegates in this tree (confirmed by
    // inspection: the target items exist, are visible, but findChild still
    // returns null), so all lookups in this file walk the tree by hand.
    function findByObjectName(root, name) {
        if (!root) return null
        if (root.objectName === name) return root
        if (root.children) {
            for (const child of root.children) {
                const found = findByObjectName(child, name)
                if (found) return found
            }
        }
        return null
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
        const item = findByObjectName(dialog.contentItem, "categoryItem_" + categoryName)
        verify(item !== null, "no category item named 'categoryItem_" + categoryName + "' found in SettingsDialog")
        mouseClick(item)
        // StackLayout doesn't flip the new page's `visible` synchronously
        // with the click, so give it a render pass before inspecting it.
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never re-rendered after selecting category '" + categoryName + "'")
    }

    function test_kovaaksDirField_showsSettingsVmDirWithoutFileScheme() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({kovaaksDir: "file:///D:/CustomDir"}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const field = findByObjectName(dialog.contentItem, "kovaaksDirField")
        verify(field !== null, "no field named 'kovaaksDirField' found in SettingsDialog")
        compare(field.text, "D:/CustomDir", "kovaaksDirField should show settingsVm.kovaaksDir with the file:// scheme stripped")
    }

    function test_directoriesCategory_showsProfilePathAndLoadedState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profilePath: "file:///D:/CustomProfile/profile.pb", profileLoaded: true}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const field = findByObjectName(dialog.contentItem, "profilePathField")
        verify(field !== null, "no field named 'profilePathField' found in SettingsDialog Directories category")
        compare(field.text, "D:/CustomProfile/profile.pb", "profilePathField should show settingsVm.profilePath with the file:// scheme stripped")

        const statusLabel = findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Directories category")
        compare(statusLabel.text, "Profile status: Loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === true")
    }

    function test_directoriesCategory_showsNotLoadedWhenProfileVmReportsFalse() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: false}),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const statusLabel = findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Profile category")
        compare(statusLabel.text, "Profile status: Not loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === false")
    }

    function test_generateProfileButton_delegatesToSessionVm() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const button = findByObjectName(dialog.contentItem, "generateProfileButton")
        verify(button !== null, "no button named 'generateProfileButton' found in SettingsDialog Profile category")
        mouseClick(button)

        compare(dialog.sessionVm.generateProfileCalls, 1, "clicking generateProfileButton should call sessionVm.generateProfile() once")
    }

    function test_profileBuildProgressBar_hiddenWhenNoBuildIsRunning() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            visualSettings: visualSettings
        })

        const bar = findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        verify(bar !== null, "no progress bar named 'profileBuildProgressBar' found in SettingsDialog Profile category")
        compare(bar.visible, false, "the progress bar should be hidden while no build is running")
    }

    function test_profileBuildProgressBar_showsProgressWhileBuilding() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true, profileBuildProgress: 0.4}),
            visualSettings: visualSettings
        })

        const bar = findByObjectName(dialog.contentItem, "profileBuildProgressBar")
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

        const bar = findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        compare(bar.indeterminate, true, "a build with no progress yet should render as indeterminate")
    }

    function test_seriesEnabledSwitchesListAllSeriesConfigsAndReflectEnabledState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        selectCategory(dialog, "Graph Lines")

        const scoreSwitch = findByObjectName(dialog.contentItem, "seriesEnabledSwitch_1")
        const accuracySwitch = findByObjectName(dialog.contentItem, "seriesEnabledSwitch_2")
        verify(scoreSwitch !== null)
        verify(accuracySwitch !== null)
        compare(scoreSwitch.checked, true)
        compare(accuracySwitch.checked, false)
    }

    function test_togglingSeriesEnabledSwitchWritesThroughSettingsVm() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        selectCategory(dialog, "Graph Lines")

        const accuracySwitch = findByObjectName(dialog.contentItem, "seriesEnabledSwitch_2")
        verify(accuracySwitch !== null)
        mouseClick(accuracySwitch)

        tryCompare(accuracySwitch, "checked", true)
        // SeriesConfigDraftPanel's settingsVm is a distinct object from dialog.settingsVm despite
        // the plain `settingsVm: root.settingsVm` forwarding binding in SettingsDialog.qml — read
        // call-site state back through the panel's own settingsVm, matching
        // tst_SeriesConfigDraftPanel.qml's convention, not through the dialog's.
        const panel = findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
        verify(panel !== null)
        tryCompare(panel.settingsVm, "setSeriesEnabledCalls", 1)
        compare(panel.settingsVm.lastSetSeriesEnabledId, "2")
        compare(panel.settingsVm.lastSetSeriesEnabledValue, true)
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
        compare(dialog.settingsVm.discardSeriesDraftCalls, 0)
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

        tryCompare(dialog.settingsVm, "discardSeriesDraftCalls", 1)
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
        compare(dialog.settingsVm.discardSeriesDraftCalls, 0)
    }

    function test_switchingCategoriesDoesNotRetriggerDraftLifecycle() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        selectCategory(dialog, "Graph Lines")
        selectCategory(dialog, "Directories")
        selectCategory(dialog, "Graph Lines")

        compare(dialog.settingsVm.discardSeriesDraftCalls, 0)
    }

    function test_openingDialogCallsBeginSeriesDraft() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
        })

        const panel = findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
        verify(panel !== null, "no item named 'seriesConfigDraftPanel' found in SettingsDialog")
        compare(dialog.settingsVm.beginDraftCalls, 1, "beginSeriesDraft should be called when the dialog is opened")
    }

    function test_reopeningDialogAfterCloseCallsBeginSeriesDraftAgain() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: false}),
            sessionVm: makeFakeSessionVm(),
        })

        const panel = findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
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

        const panel = findByObjectName(dialog.contentItem, "seriesConfigDraftPanel")
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
