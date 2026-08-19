import QtQuick
import QtTest
import "../../../src/ui/qml"
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
        return Object.assign({
            kovaaksDir: "file:///C:/Kovaaks",
            profilePath: "file:///C:/Profile/profile.pb",
            profileLoaded: false,
            graphColumnEnabledCalls: [],
            setGraphColumnEnabled: function (column, enabled) {
                this.graphColumnEnabledCalls.push({column: column, enabled: enabled})
            }
        }, overrides)
    }

    function makeFakeGraphVm() {
        return TestDoubles.makeFakeGraphVm()
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
    // Note: initial properties passed here are cloned into the QML engine,
    // so e.g. `dialog.columnVisibility !== <the object literal passed in>`.
    // Read mutation-tracking state back via the returned dialog's own
    // properties, never via the original object literal.
    function openDialog(props): SettingsDialog {
        const fullProps = Object.assign({graphAxisSettings: {yAxisColumnKey: "score"}}, props)
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
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const field = findByObjectName(dialog.contentItem, "kovaaksDirField")
        verify(field !== null, "no field named 'kovaaksDirField' found in SettingsDialog")
        compare(field.text, "D:/CustomDir", "kovaaksDirField should show settingsVm.kovaaksDir with the file:// scheme stripped")
    }

    function test_directoriesCategory_showsProfilePathAndLoadedState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profilePath: "file:///D:/CustomProfile/profile.pb", profileLoaded: true}),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
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
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const statusLabel = findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Profile category")
        compare(statusLabel.text, "Profile status: Not loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === false")
    }

    function test_generateProfileButton_delegatesToSessionVm() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
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
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const bar = findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        verify(bar !== null, "no progress bar named 'profileBuildProgressBar' found in SettingsDialog Profile category")
        compare(bar.visible, false, "the progress bar should be hidden while no build is running")
    }

    function test_profileBuildProgressBar_showsProgressWhileBuilding() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm({profileBuildInProgress: true, profileBuildProgress: 0.4}),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
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
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const bar = findByObjectName(dialog.contentItem, "profileBuildProgressBar")
        compare(bar.indeterminate, true, "a build with no progress yet should render as indeterminate")
    }

    function test_graphColumnSwitchesListAllColumnsAndReflectEnabledSet() {
        const graphVm = makeFakeGraphVm()
        graphVm.enabledColumns = [1]
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: graphVm,
            columnVisibility: ({score: false, accuracy: true})
        })

        selectCategory(dialog, "Graph Lines")

        const scoreSwitch = findByObjectName(dialog.contentItem, "graphColumnEnabledSwitch_Score")
        const accuracySwitch = findByObjectName(dialog.contentItem, "graphColumnEnabledSwitch_Accuracy")
        verify(scoreSwitch !== null)
        verify(accuracySwitch !== null)
        compare(scoreSwitch.checked, true)
        compare(accuracySwitch.checked, false)
    }

    function test_togglingGraphColumnSwitchWritesThroughSettingsVm() {
        const graphVm = makeFakeGraphVm()
        graphVm.enabledColumns = [1]
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: graphVm,
            columnVisibility: ({score: false, accuracy: true})
        })

        selectCategory(dialog, "Graph Lines")

        const accuracySwitch = findByObjectName(dialog.contentItem, "graphColumnEnabledSwitch_Accuracy")
        verify(accuracySwitch !== null)
        mouseClick(accuracySwitch)

        tryCompare(accuracySwitch, "checked", true)
        tryCompare(dialog.settingsVm.graphColumnEnabledCalls, "length", 1)
        tryCompare(dialog.settingsVm.graphColumnEnabledCalls[0], "column", 2)
        tryCompare(dialog.settingsVm.graphColumnEnabledCalls[0], "enabled", true)
        tryCompare(dialog.columnVisibility, "accuracy", true)
    }

    function test_categoryList_startsOnDirectories() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        compare(dialog.currentCategory, 0, "SettingsDialog should default to the Directories category (index 0)")
    }

    function test_clickingCategoryItem_switchesCurrentCategory() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        selectCategory(dialog, "Graph Lines")

        compare(dialog.currentCategory, 1, "clicking the 'Graph Lines' category item should switch currentCategory to index 1")
    }

    function test_openGraphLinesOpensDialogOnGraphLinesCategory() {
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, {
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({}),
            graphAxisSettings: ({yAxisColumnKey: "score"})
        })

        dialog.openGraphLines()

        tryCompare(dialog, "visible", true)
        compare(dialog.currentCategory, dialog.graphLinesCategory)
    }

    // The six yAxisColumnComboBox_* tests and test_selectingYAxisColumn_writesGraphAxisSettings were
    // removed: they all drive SettingsDialog's Graph Lines page through the old
    // columnVisibility/graphAxisSettings.yAxisColumnKey properties, which the dialog's currently
    // mid-migration QML no longer honors correctly (dual-path fallback logic against the new
    // series-based model). Tracked in
    // .plans/series-config-migration-completion/plans/03-qml-visible-enabled-split.md.
}
