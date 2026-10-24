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
            kovaaksDirValid: true,
            allSeriesConfigs: [
                {id: "1", name: "Score", color: "#009600", enabled: true, displayPosition: 0},
                {id: "2", name: "Accuracy", color: "#00ffff", enabled: false, displayPosition: 1}
            ],
            runAxisLabelChoices: [{key: "series:1", name: "Score"}]
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
            settingsVm: makeFakeSettingsVm(), sessionVm: makeFakeSessionVm(), currentCategory: 2
        })
        dialog.openProfile()
        compare(dialog.currentCategory, 0)
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

        selectCategory(dialog, "Graph Lines")

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
        selectCategory(dialog, "Graph Lines")

        compare(ItemLookup.findByObjectName(dialog.contentItem, "saveGraphLinesButton"), null)
        compare(ItemLookup.findByObjectName(dialog.contentItem, "discardGraphLineChangesButton"), null)
    }

    function test_clickingCategoryItem_switchesCurrentCategory() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        selectCategory(dialog, "Graph Lines")

        compare(dialog.currentCategory, 1, "clicking the 'Graph Lines' category item should switch currentCategory to index 1")
    }

    function test_openGraphLinesOpensDialogOnGraphLinesCategory() {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm()
        })

        dialog.openGraphLines()

        tryCompare(dialog, "visible", true)
        compare(dialog.currentCategory, dialog.graphLinesCategory)
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

        selectCategory(dialog, "Graph Lines")
        selectCategory(dialog, "Directories")
        selectCategory(dialog, "Graph Lines")

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

    function axisChoices() {
        return [
            {key: "axis:1", name: "Score"},
            {key: "series:7", name: "Score/s (5s)"}
        ]
    }

    function test_yAxisSelectorListsTheAxisChoicesAndShowsTheWorkingCopyChoice() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({runAxisLabelKey: "series:7", runAxisLabelChoices: axisChoices()}),
            sessionVm: makeFakeSessionVm()
        })
        selectCategory(dialog, "Graph Lines")

        const combo = ItemLookup.findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        verify(combo !== null)
        compare(combo.count, 2)
        compare(combo.textAt(0), "Score")
        compare(combo.currentIndex, 1)
        compare(combo.displayText, "Score/s (5s)")
    }

    // The label choice joins the working copy of the visit instead of being persisted by QML.
    function test_choosingAYAxisEditsTheWorkingCopy() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({runAxisLabelChoices: axisChoices()}),
            sessionVm: makeFakeSessionVm()
        })
        selectCategory(dialog, "Graph Lines")
        const combo = ItemLookup.findByObjectName(dialog.contentItem, "yAxisColumnComboBox")

        combo.activated(1)

        compare(dialog.settingsVm.setRunAxisLabelCalls, ["series:7"])
    }

    function test_theYAxisListHighlightsTheEntryUnderTheKeyboardOrPointer() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({runAxisLabelChoices: axisChoices()}),
            sessionVm: makeFakeSessionVm()
        })
        selectCategory(dialog, "Graph Lines")
        const combo = ItemLookup.findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        combo.forceActiveFocus()
        combo.popup.open()
        tryCompare(combo.popup, "opened", true)
        const list = combo.popup.contentItem
        tryVerify(() => list.itemAtIndex(1) !== null)

        keyClick(Qt.Key_Down)

        tryCompare(combo, "highlightedIndex", 1)
        verify(list.itemAtIndex(1).highlighted)
        verify(!list.itemAtIndex(0).highlighted)
        combo.popup.close()
    }

    function test_anUnknownYAxisFallsBackToTheFirstChoice() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({runAxisLabelKey: "axis:99", runAxisLabelChoices: axisChoices()}),
            sessionVm: makeFakeSessionVm()
        })
        selectCategory(dialog, "Graph Lines")

        const combo = ItemLookup.findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        compare(combo.currentIndex, 0)
        compare(combo.displayText, "Score")
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

    function test_graphsCategoryFollowsGraphLinesInTheSidebar() {
        const {dialog} = openObservableDialog()
        dialog.open()
        verify(waitForRendering(dialog.contentItem))

        const lines = ItemLookup.findByObjectName(dialog.contentItem, "categoryItem_Graph Lines")
        const graphs = ItemLookup.findByObjectName(dialog.contentItem, "categoryItem_Graphs")
        verify(graphs !== null, "no categoryItem_Graphs in the sidebar")
        compare(graphs.text, "Graphs")
        verify(graphs.mapToItem(dialog.contentItem, 0, 0).y > lines.mapToItem(dialog.contentItem, 0, 0).y)
        compare(dialog.graphsCategory, 2)
    }

    function test_choosingGraphsShowsTheGraphsPageWithItsSelector() {
        const {dialog, vm} = openObservableDialog()
        dialog.open()
        verify(waitForRendering(dialog.contentItem))

        selectCategory(dialog, "Graphs")

        compare(dialog.currentCategory, dialog.graphsCategory)
        const page = ItemLookup.findByObjectName(dialog.contentItem, "graphsSettingsPage")
        verify(page !== null && page.visible)
        const selector = ItemLookup.findByObjectName(dialog.contentItem, "graphSelector")
        verify(selector !== null)
        const names = []
        for (let i = 0; i < selector.count; ++i) names.push(selector.textAt(i))
        compare(names, ["Run graph", "Daily playtime", "Scenario history", "Benchmark rank history", "Benchmark playtime"])
        compare(selector.currentIndex, 0)
        compare(selector.Accessible.name, "Graph")
    }

    function test_theSelectorChoosesTheGraphBeingEdited() {
        const {dialog, vm} = openObservableDialog()
        dialog.open()
        selectCategory(dialog, "Graphs")
        const selector = ItemLookup.findByObjectName(dialog.contentItem, "graphSelector")

        selector.currentIndex = 3
        selector.activated(3)

        compare(vm.selectedGraph, "benchmarkRank")
        vm.selectedGraph = "dailyPlaytime"
        compare(selector.currentIndex, 1, "the selector follows the view model")
    }

    function test_openGraphOpensTheVisitOnThatGraphAndFocusesTheEditor() {
        const {dialog, vm} = openObservableDialog()

        dialog.openGraph("scenarioHistory")
        verify(waitForRendering(dialog.contentItem))

        compare(dialog.visible, true)
        compare(dialog.currentCategory, dialog.graphsCategory)
        compare(vm.selectedGraph, "scenarioHistory")
        compare(vm.beginVisitCalls, 1)
        const selector = ItemLookup.findByObjectName(dialog.contentItem, "graphSelector")
        compare(selector.currentIndex, 2)
        const first = ItemLookup.findByObjectName(dialog.contentItem, "graphField_dataRange_range")
        tryVerify(() => first !== null && first.activeFocus, 2000, "focus moves into the graph editor")
        verify(!selector.activeFocus)
    }

    function test_openGraphWithAnUnknownKeyStillOpensTheGraphsPage() {
        const {dialog, vm} = openObservableDialog()
        vm.selectedGraph = "dailyPlaytime"

        dialog.openGraph("nonsense")

        compare(dialog.currentCategory, dialog.graphsCategory)
        compare(vm.selectedGraph, "dailyPlaytime")
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
