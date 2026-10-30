import QtQuick
import QtTest
import QtQuick.Dialogs
import QtCore
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

    Settings { id: testSettings; category: "window" }
    function init() {
        testSettings.setValue("settingsDialogPage", "")
        testSettings.setValue("settingsDialogSection", "")
        testSettings.sync()
    }

    Component {
        id: settingsDialogComponent
        SettingsDialog {}
    }

    function makeFakeSettingsVm(overrides) {
        return TestDoubles.makeFakeSettingsVm(Object.assign({
            kovaaksDir: "file:///C:/Kovaaks",
            profilePath: "file:///C:/Profile/profile.pb",
            profileLoaded: false,
            kovaaksDirValid: true,
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
            property bool profileImportBusy: false
            property double profileImportProgress: 0
            property int profileImportDone: 0
            property int profileImportTotal: 0
            property string profileImportMode: "rebuild"
            property string profileImportPhase: "importing"
            property string profileImportOutcome: "none"
            property int profileImportAdded: 0
            property int profileImportEnriched: 0
            property var profileImportIssues: []
            property bool profileImportSaveFailed: false
            property int refreshCalls: 0
            function refreshProfile() { refreshCalls++ }
            function generateProfile() {}
        }
    }

    function makeFakeSessionVm(overrides) {
        return Object.assign({
            generateProfileCalls: 0,
            generateProfile: function () { this.generateProfileCalls++ },
            profileImportBusy: false,
            profileImportProgress: 0,
            profileImportDone: 0,
            profileImportMode: "rebuild",
            profileImportPhase: "importing",
            profileImportOutcome: "none",
            profileImportAdded: 0,
            profileImportEnriched: 0,
            profileImportIssues: [],
            profileImportSaveFailed: false,
            profileImportTotal: 0
        }, overrides)
    }

    function test_refreshButtonDelegatesToSessionVm() {
        const vm = createTemporaryObject(observableSessionVmComponent, testCase)
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: vm})
        const button = ItemLookup.findByObjectName(dialog.contentItem, "refreshProfileButton")
        verify(button !== null)
        mouseClick(button)
        compare(vm.refreshCalls, 1)
    }

    function test_refreshAndRebuildDisableTogether() {
        const vm = createTemporaryObject(observableSessionVmComponent, testCase)
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: vm})
        vm.profileImportBusy = true
        compare(ItemLookup.findByObjectName(dialog.contentItem, "refreshProfileButton").enabled, false)
        compare(ItemLookup.findByObjectName(dialog.contentItem, "generateProfileButton").enabled, false)
    }

    // Opens the dialog and waits for its Popup content (created on open,
    // not at construction) to actually show up in the item tree.
    function openDialog(props): SettingsDialog {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, props)
        verify(dialog !== null, "SettingsDialog failed to instantiate")
        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after open()")
        return dialog
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
            sessionVm: makeFakeSessionVm()
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
            sessionVm: makeFakeSessionVm()
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

    function test_generateProfileButton_asksForConfirmationBeforeRebuilding() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        const button = ItemLookup.findByObjectName(dialog.contentItem, "generateProfileButton")
        verify(button !== null, "no button named 'generateProfileButton' found in SettingsDialog Profile category")
        mouseClick(button)

        tryCompare(dialog.rebuildProfilePrompt, "visible", true)
        compare(dialog.sessionVm.generateProfileCalls, 0, "the rebuild must wait for the confirmation")
        dialog.rebuildProfilePrompt.close()
    }

    function test_rebuildPrompt_okRebuildsAndCancelDoesNot() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        dialog.rebuildProfilePrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)
        compare(dialog.sessionVm.generateProfileCalls, 0)
        dialog.rebuildProfilePrompt.buttonClicked(MessageDialog.Ok, MessageDialog.AcceptRole)
        compare(dialog.sessionVm.generateProfileCalls, 1)
    }

    function test_refreshAndRebuildShareOneActionRow() {
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm()})
        const refresh = ItemLookup.findByObjectName(dialog.contentItem, "refreshProfileButton")
        const rebuild = ItemLookup.findByObjectName(dialog.contentItem, "generateProfileButton")
        compare(refresh.parent, rebuild.parent)
    }

    function resultLabelText(overrides) {
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm(overrides)})
        return ItemLookup.findByObjectName(dialog.contentItem, "profileResultLabel").text
    }

    function test_profileResultLabel_summarisesTheLastImport() {
        compare(resultLabelText({profileImportOutcome: "succeeded", profileImportAdded: 3}), "Added 3 runs")
        compare(resultLabelText({profileImportOutcome: "succeeded", profileImportAdded: 1, profileImportEnriched: 2}),
                "Added 1 run; updated 2 runs")
        compare(resultLabelText({profileImportOutcome: "succeeded", profileImportEnriched: 1}), "Updated 1 run")
        compare(resultLabelText({profileImportOutcome: "succeeded"}), "Up to date")
        compare(resultLabelText({profileImportOutcome: "partial", profileImportIssues: [{path: "a.perf", message: "x"}]}),
                "No new runs imported")
        compare(resultLabelText({profileImportOutcome: "failed", profileImportSaveFailed: true}),
                "Save failed — previous profile unchanged")
        compare(resultLabelText({profileImportOutcome: "failed"}), "Refresh failed")
        compare(resultLabelText({profileImportOutcome: "none"}), "")
    }

    function test_profileResultLabel_showsProgressWhileBusy() {
        compare(resultLabelText({profileImportBusy: true, profileImportDone: 12, profileImportTotal: 40}), "12 of 40 runs")
        compare(resultLabelText({profileImportBusy: true}), "Scanning sources…")
        compare(resultLabelText({profileImportBusy: true, profileImportPhase: "saving", profileImportTotal: 5}), "Saving…")
    }

    function test_profileIssuesFrame_hiddenWithoutIssues() {
        const dialog = openDialog({settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm()})
        compare(ItemLookup.findByObjectName(dialog.contentItem, "profileIssuesFrame").visible, false)
    }

    function test_profileIssuesFrame_listsEveryIssueByFileName() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileImportIssues: [
                {path: "C:/a/b/broken.perf", message: "Incomplete file", retryable: true},
                {path: "C:\\a\\old.perf", message: "Unreadable", retryable: false, previouslyFailed: true}
            ]})
        })
        const frame = ItemLookup.findByObjectName(dialog.contentItem, "profileIssuesFrame")
        compare(frame.visible, true)
        compare(ItemLookup.findByObjectName(dialog.contentItem, "profileIssuesHeader").text,
                "2 source files could not be imported")
        const text = ItemLookup.findByObjectName(dialog.contentItem, "profileIssueText")
        tryVerify(() => text !== null && text.text !== "")
        compare(text.text, "broken.perf: Incomplete file\nold.perf: Unreadable")
        compare(text.readOnly, true)
        compare(text.selectByMouse, true)
    }

    function test_openProfile_selectsTheProfilePageAndShowsTheDialog() {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {
            settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm(), currentPage: "run"
        })
        dialog.openProfile()
        compare(dialog.currentPage, "profile")
        compare(dialog.visible, true)
    }

    function test_profileBuildProgressBar_hiddenWhenNoBuildIsRunning() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        const bar = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        verify(bar !== null, "no progress bar named 'profileBuildProgressBar' found in SettingsDialog Profile category")
        compare(bar.visible, false, "the progress bar should be hidden while no build is running")
    }

    function test_profileBuildProgressBar_showsProgressWhileBuilding() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileImportBusy: true, profileImportProgress: 0.4,
                                          profileImportDone: 2, profileImportTotal: 5})
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
            sessionVm: makeFakeSessionVm({profileImportBusy: true})
        })

        const bar = ItemLookup.findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        compare(bar.indeterminate, true, "a build with no progress yet should render as indeterminate")
    }

    function test_profileStatus_saysRebuildingWhileABuildRuns_andRestoresAfter() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: true}),
            sessionVm: createTemporaryObject(observableSessionVmComponent, testCase, {profileImportBusy: true})
        })
        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        compare(statusLabel.text, "Profile status: Rebuilding", "a running rebuild replaces the loaded state in the status line")

        dialog.sessionVm.profileImportBusy = false
        compare(statusLabel.text, "Profile status: Loaded", "the status line shows the real state again once the rebuild ends")
    }

    function test_profileStatus_notLoadedAfterAFailedRebuild() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: false}),
            sessionVm: createTemporaryObject(observableSessionVmComponent, testCase, {profileImportBusy: true})
        })
        const statusLabel = ItemLookup.findByObjectName(dialog.contentItem, "profileLoadedLabel")
        compare(statusLabel.text, "Profile status: Rebuilding")

        dialog.sessionVm.profileImportBusy = false
        compare(statusLabel.text, "Profile status: Not loaded")
    }

    function test_seriesEnabledSwitchesListAllSeriesConfigsAndReflectEnabledState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        dialog.openGraphLines()

        const scoreSwitch = ItemLookup.findByObjectName(dialog.contentItem, "seriesEnabledSwitch_1")
        const accuracySwitch = ItemLookup.findByObjectName(dialog.contentItem, "seriesEnabledSwitch_2")
        verify(scoreSwitch !== null)
        verify(accuracySwitch !== null)
        compare(scoreSwitch.checked, true)
        compare(accuracySwitch.checked, false)
    }

    function test_graphLinesPageHasNoSaveOrDiscardButtonsOfItsOwn() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })
        dialog.openGraphLines()

        compare(ItemLookup.findByObjectName(dialog.contentItem, "saveGraphLinesButton"), null)
        compare(ItemLookup.findByObjectName(dialog.contentItem, "discardGraphLineChangesButton"), null)
    }

    function test_openGraphLinesOpensRunGraph() {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        dialog.openGraphLines()

        tryCompare(dialog, "visible", true)
        compare(dialog.currentPage, "run")
    }

    function findInDialog(dialog, name) {
        return ItemLookup.findByObjectName(dialog.contentItem, name)
            || ItemLookup.findByObjectName(dialog.footer, name)
    }

    function test_openingTheDialogBeginsAVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        compare(dialog.settingsVm.beginVisitCalls, 1)
    }

    // The view model keeps an already open visit as it is; the dialog only has to keep asking.
    function test_reopeningAnOpenDialogNeitherEndsNorSavesItsVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.open()

        compare(dialog.visible, true)
        compare(dialog.settingsVm.endVisitCalls, 0)
        compare(dialog.settingsVm.saveCalls, 0)
    }

    function test_closingACleanVisitEndsItWithoutPrompting() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: false}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()

        tryCompare(dialog, "visible", false)
        compare(dialog.discardChangesPrompt.visible, false)
        compare(dialog.settingsVm.endVisitCalls, 1)
    }

    function test_closingWithPendingChangesShowsConfirmPromptAndStaysOpenUntilResolved() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()

        tryCompare(dialog, "visible", true)
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        compare(dialog.settingsVm.endVisitCalls, 0)
    }

    function test_discardingOnCloseEndsTheVisitWithoutSavingThenCloses() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)

        tryCompare(dialog, "visible", false)
        compare(dialog.settingsVm.saveCalls, 0)
        verify(dialog.settingsVm.endVisitCalls >= 1)
    }

    function test_savingOnCloseSavesThenClosesOnceAccepted() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        tryCompare(dialog, "visible", false)
        compare(dialog.settingsVm.saveCalls, 1)
        compare(dialog.settingsVm.endVisitCalls, 1)
    }

    function test_aSaveRejectedOnCloseKeepsTheWindowAndItsVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({
                pendingChanges: true,
                saveResult: {accepted: false, issues: [{graph: "run", group: "graphLines", target: "", field: "", code: "invalidRunSeries"}]}
            }),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)

        wait(50)
        compare(dialog.visible, true)
        compare(dialog.settingsVm.saveCalls, 1)
        compare(dialog.settingsVm.endVisitCalls, 0)
        compare(dialog.settingsVm.pendingChanges, true)
    }

    function test_cancellingCloseLeavesWindowOpenAndVisitUntouched() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)

        compare(dialog.visible, true)
        compare(dialog.settingsVm.saveCalls, 0)
        compare(dialog.settingsVm.endVisitCalls, 0)
    }

    function test_footerSaveSavesTheVisitAndKeepsTheWindowOpen() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })
        const save = findInDialog(dialog, "settingsSaveButton")
        verify(save !== null, "the footer offers Save")

        mouseClick(save)

        compare(dialog.settingsVm.saveCalls, 1)
        compare(dialog.visible, true)
        compare(dialog.settingsVm.endVisitCalls, 0)
    }

    // Disabling the Save button while it holds focus moves activeFocusItem, which its own `enabled` binding reads.
    function test_saveButtonDisablingWhileFocusedIsNotABindingLoop() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })
        const save = findInDialog(dialog, "settingsSaveButton")
        save.forceActiveFocus()
        verify(save.activeFocus)

        failOnWarning(/Binding loop/)
        dialog.settingsVm = Object.assign({}, dialog.settingsVm, {pendingChanges: false})

        compare(save.enabled, false)
    }

    function test_footerCancelDropsTheVisitAndClosesWithoutPrompting() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })
        const cancel = findInDialog(dialog, "settingsCancelButton")
        verify(cancel !== null, "the footer offers Cancel")

        mouseClick(cancel)

        tryCompare(dialog, "visible", false)
        compare(dialog.discardChangesPrompt.visible, false)
        compare(dialog.settingsVm.saveCalls, 0)
        verify(dialog.settingsVm.endVisitCalls >= 1)
    }

    function test_switchingCategoriesKeepsTheVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.openGraphLines()
        dialog.openProfile()
        dialog.openGraphLines()

        compare(dialog.settingsVm.beginVisitCalls, 1)
        compare(dialog.settingsVm.endVisitCalls, 0)
        compare(dialog.settingsVm.saveCalls, 0)
    }

    function test_reopeningAfterACleanCloseBeginsAFreshVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: false}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog, "visible", false)
        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after reopening")

        compare(dialog.settingsVm.endVisitCalls, 1)
        compare(dialog.settingsVm.beginVisitCalls, 2)
    }

    function test_reopeningAfterSavingOnCloseBeginsAFreshVisit() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({pendingChanges: true}),
            sessionVm: makeFakeSessionVm()
        })

        dialog.close()
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        dialog.discardChangesPrompt.buttonClicked(MessageDialog.Save, MessageDialog.AcceptRole)
        tryCompare(dialog, "visible", false)

        dialog.open()
        verify(waitForRendering(dialog.contentItem), "SettingsDialog contentItem never became visible after reopening")

        compare(dialog.settingsVm.beginVisitCalls, 2)
    }

    // Names commit on editingFinished, so until the field loses focus the visit holds no change.
    function openEditingDialog() {
        const {dialog, vm} = openObservableDialog()
        vm.allSeriesConfigs = [{id: "1", name: "Score", color: "#009600", width: 2, enabled: true, displayPosition: 0,
                                hiddenLeadingPoints: 0, yAxisId: "", expression: "SCORE"}]
        vm.axisOverview = {shownKey: "axis:1", independentLines: [], sharedAxes: [
            {id: "1", key: "axis:1", name: "Score", nameText: "Score", nameIssue: "", lineIds: [],
             lineNames: [], hasEnabledLine: false, shown: true}
        ]}
        dialog.open()
        verify(waitForRendering(dialog.contentItem))
        dialog.openGraphLines()
        return {dialog: dialog, vm: vm}
    }

    function startTyping(dialog, fieldName, text) {
        const field = findInDialog(dialog, fieldName)
        verify(field !== null, "no field " + fieldName)
        field.forceActiveFocus()
        field.text = text
        compare(dialog.activeFocusItem, field)
        compare(dialog.settingsVm.calls.length, 0)
    }

    function startTypingAnAxisName(dialog) {
        dialog.jumpToSection("axes")
        startTyping(dialog, "axisNameField_1", "Points")
    }

    function test_closingWhileAnAxisNameIsBeingTypedOffersToSaveIt() {
        const {dialog, vm} = openEditingDialog()
        startTypingAnAxisName(dialog)

        dialog.close()

        compare(vm.callsNamed("renameAxis").map(c => c.args), [["1", "Points"]])
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        compare(vm.endVisitCalls, 0)
    }

    function test_closingWhileALineNameIsBeingTypedOffersToSaveIt() {
        const {dialog, vm} = openEditingDialog()
        startTyping(dialog, "seriesNameField_1", "Total")

        dialog.close()

        compare(vm.callsNamed("updateComputedSeries").map(c => c.args), [["1", "Total"]])
        tryCompare(dialog.discardChangesPrompt, "visible", true)
        compare(vm.endVisitCalls, 0)
    }

    function test_saveCommitsAnAxisNameThatIsStillBeingTyped() {
        const {dialog, vm} = openEditingDialog()
        startTypingAnAxisName(dialog)
        const save = findInDialog(dialog, "settingsSaveButton")
        verify(save.enabled, "Save is offered while a name is being typed")

        mouseClick(save)

        compare(vm.callsNamed("renameAxis").map(c => c.args), [["1", "Points"]])
        compare(vm.saveCalls, 1)
    }

    // A pointer press hands Save the focus, which commits the field by itself; activation that leaves
    // focus in the field relies on Save committing it before saving.
    function test_saveActivatedWhileTheFieldKeepsFocusCommitsItFirst() {
        const {dialog, vm} = openEditingDialog()
        startTypingAnAxisName(dialog)
        const field = findInDialog(dialog, "axisNameField_1")

        findInDialog(dialog, "settingsSaveButton").clicked()

        compare(dialog.activeFocusItem, field)
        compare(vm.callsNamed("renameAxis").map(c => c.args), [["1", "Points"]])
        compare(vm.saveCalls, 1)
        // The fake's commit marks the visit dirty and its save cleans it, so this holds only if the commit came first.
        compare(vm.pendingChanges, false)
    }

    function test_focusingANameWithoutTypingLeavesSaveDisabled() {
        const {dialog, vm} = openEditingDialog()
        dialog.jumpToSection("axes")
        const field = findInDialog(dialog, "axisNameField_1")

        field.forceActiveFocus()

        compare(dialog.activeFocusItem, field)
        compare(findInDialog(dialog, "settingsSaveButton").enabled, false)
    }

    function test_typingANameBackToItsSavedTextDisablesSaveAgain() {
        const {dialog, vm} = openEditingDialog()
        startTypingAnAxisName(dialog)
        const save = findInDialog(dialog, "settingsSaveButton")
        verify(save.enabled)

        findInDialog(dialog, "axisNameField_1").text = "Score"

        compare(save.enabled, false)
    }

    // Graphs-page fields report every edit as it is typed, so focusing one leaves nothing to commit.
    function test_focusingAGraphsFieldLeavesSaveDisabled() {
        const {dialog} = openObservableDialog()
        dialog.openGraph("dailyPlaytime")
        verify(waitForRendering(dialog.contentItem))
        const field = findInDialog(dialog, "graphField_dataRange_averagingWindow")
        verify(field !== null)

        field.forceActiveFocus()

        tryCompare(field, "activeFocus", true)
        compare(findInDialog(dialog, "settingsSaveButton").enabled, false)
    }

    function test_closingWithAnUntouchedNameFocusedCommitsNothing() {
        const {dialog, vm} = openEditingDialog()
        const field = findInDialog(dialog, "seriesNameField_1")
        field.forceActiveFocus()
        compare(dialog.activeFocusItem, field)

        dialog.close()

        compare(vm.callsNamed("updateComputedSeries").length, 0)
        compare(vm.endVisitCalls, 1)
        tryCompare(dialog, "visible", false)
    }

    function openDialogRejectingAnAxisName() {
        const issue = {graph: "run", group: "graphLines", target: "axis:2", field: "name", code: "axisNameDuplicate"}
        return openDialog({
            settingsVm: makeFakeSettingsVm({
                selectedGraph: "run",
                pendingChanges: true,
                saveResult: {accepted: false, issues: [issue]},
                axisOverview: {shownKey: "axis:1", independentLines: [], sharedAxes: [
                    {id: "1", key: "axis:1", name: "Score", nameText: "Score", nameIssue: "", lineIds: [],
                     lineNames: [], hasEnabledLine: false, shown: true},
                    {id: "2", key: "axis:2", name: "Accuracy", nameText: "score", nameIssue: "axisNameDuplicate",
                     lineIds: [], lineNames: [], hasEnabledLine: false, shown: false}
                ]}
            }),
            sessionVm: makeFakeSessionVm()
        })
    }

    // Focus lands before Save returns: the tab and page switches show the field synchronously.
    function test_aSaveRejectedForAnAxisNameOpensTheAxesTabOnThatField() {
        const dialog = openDialogRejectingAnAxisName()
        dialog.openGraphLines()
        mouseClick(findInDialog(dialog, "settingsSaveButton"))

        compare(dialog.currentSection, "axes")
        compare(dialog.activeFocusItem.objectName, "axisNameField_2")
    }

    function test_aSaveRejectedForAnAxisNameFromAnotherPageFocusesThatField() {
        const dialog = openDialogRejectingAnAxisName()
        compare(dialog.currentPage, "profile")

        mouseClick(findInDialog(dialog, "settingsSaveButton"))

        compare(dialog.currentPage, "run")
        compare(dialog.activeFocusItem.objectName, "axisNameField_2")
    }

    // ---- Graphs category ---------------------------------------------------------------------------

    Component {
        id: observableSettingsVmComponent
        FakeQuickSettingsVm {}
    }

    function openObservableDialog() {
        const vm = createTemporaryObject(observableSettingsVmComponent, testCase)
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {settingsVm: vm, sessionVm: makeFakeSessionVm()})
        verify(dialog !== null)
        return {dialog: dialog, vm: vm}
    }

    function test_openGraphOpensTheVisitOnThatGraphAndFocusesTheEditor() {
        const {dialog, vm} = openObservableDialog()

        dialog.openGraph("scenarioHistory")
        verify(waitForRendering(dialog.contentItem))

        compare(dialog.visible, true)
        compare(dialog.currentPage, "scenarioHistory")
        compare(vm.selectedGraph, "scenarioHistory")
        compare(vm.beginVisitCalls, 1)
        const first = ItemLookup.findByObjectName(dialog.contentItem, "graphField_dataRange_range")
        tryVerify(() => first !== null && first.activeFocus, 2000, "focus moves into the graph editor")
    }

    function test_openGraphWithAnUnknownKeyOpensTheRememberedPlace() {
        const {dialog, vm} = openObservableDialog()
        vm.selectedGraph = "dailyPlaytime"

        dialog.openGraph("nonsense")

        compare(dialog.currentPage, "profile")
        compare(vm.selectedGraph, "dailyPlaytime")
    }

    function test_aRememberedSectionTheGraphNoLongerOffersKeepsTheGraph() {
        const {dialog, vm} = openObservableDialog()
        testSettings.setValue("settingsDialogPage", "dailyPlaytime")
        testSettings.setValue("settingsDialogSection", "noSuchSection")
        testSettings.sync()

        dialog.open()

        compare(dialog.currentPage, "dailyPlaytime")
        compare(vm.selectedGraph, "dailyPlaytime")
        tryCompare(dialog, "currentSection", "dataRange")
    }

    function test_openGraphOnAnAlreadyOpenDialogSwitchesGraphWithoutEndingTheVisit() {
        const {dialog, vm} = openObservableDialog()
        dialog.openGraph("dailyPlaytime")
        verify(waitForRendering(dialog.contentItem))

        dialog.openGraph("benchmarkPlaytime")

        compare(vm.selectedGraph, "benchmarkPlaytime")
        compare(vm.endVisitCalls, 0)
    }
}
