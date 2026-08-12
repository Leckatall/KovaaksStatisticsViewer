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
            profilePath: "file:///C:/Profile/profile_cache.pb",
            profileLoaded: false
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
            settingsVm: makeFakeSettingsVm({profilePath: "file:///D:/CustomProfile/profile_cache.pb", profileLoaded: true}),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const field = findByObjectName(dialog.contentItem, "profilePathField")
        verify(field !== null, "no field named 'profilePathField' found in SettingsDialog Directories category")
        compare(field.text, "D:/CustomProfile/profile_cache.pb", "profilePathField should show settingsVm.profilePath with the file:// scheme stripped")

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

    function test_columnVisibilityCheckBoxes_reflectColumnVisibility() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: false, accuracy: true})
        })

        selectCategory(dialog, "Graph Lines")

        const scoreCheckBox = findByObjectName(dialog.contentItem, "columnVisibilityCheckBox_Score")
        const accuracyCheckBox = findByObjectName(dialog.contentItem, "columnVisibilityCheckBox_Accuracy")
        verify(scoreCheckBox !== null, "no checkbox named 'columnVisibilityCheckBox_Score' found in SettingsDialog Graph Lines category")
        verify(accuracyCheckBox !== null, "no checkbox named 'columnVisibilityCheckBox_Accuracy' found in SettingsDialog Graph Lines category")
        compare(scoreCheckBox.checked, false, "Score checkbox should reflect columnVisibility.score === false")
        compare(accuracyCheckBox.checked, true, "Accuracy checkbox should reflect columnVisibility.accuracy === true")
    }

    function test_togglingColumnVisibilityCheckBox_updatesColumnVisibility() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: false, accuracy: true})
        })

        selectCategory(dialog, "Graph Lines")

        const scoreCheckBox = findByObjectName(dialog.contentItem, "columnVisibilityCheckBox_Score")
        verify(scoreCheckBox !== null, "no checkbox named 'columnVisibilityCheckBox_Score' found in SettingsDialog Graph Lines category")
        mouseClick(scoreCheckBox)

        compare(scoreCheckBox.checked, true, "clicking the Score checkbox should check it")
        compare(dialog.columnVisibility.score, true, "clicking the Score checkbox should update columnVisibility.score")
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

    function test_yAxisColumnComboBox_listsOnlyVisibleColumns() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: false}),
            graphAxisSettings: ({yAxisColumnKey: "score"})
        })

        selectCategory(dialog, "Graph Lines")

        const combo = findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        verify(combo !== null, "no combo box named 'yAxisColumnComboBox' found in SettingsDialog Graph Lines category")
        compare(combo.count, 1, "the combo box should only list the visible columns")
        compare(combo.displayText, "Score", "the combo box should show the sole visible column")
    }

    function test_selectingYAxisColumn_writesGraphAxisSettings() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: true}),
            graphAxisSettings: ({yAxisColumnKey: "score"})
        })

        selectCategory(dialog, "Graph Lines")

        const combo = findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        verify(combo !== null, "no combo box named 'yAxisColumnComboBox' found in SettingsDialog Graph Lines category")
        combo.activated(1)

        compare(dialog.graphAxisSettings.yAxisColumnKey, "accuracy", "selecting a combo entry should write graphAxisSettings.yAxisColumnKey")
    }

    function test_yAxisColumnComboBox_disabledWhenNoColumnVisible() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: false, accuracy: false}),
            graphAxisSettings: ({yAxisColumnKey: "score"})
        })

        selectCategory(dialog, "Graph Lines")

        const combo = findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        verify(combo !== null, "no combo box named 'yAxisColumnComboBox' found in SettingsDialog Graph Lines category")
        compare(combo.enabled, false, "the combo box should be disabled when no column is visible")
    }

    function test_yAxisColumnComboBox_fallsBackWithoutRewritingStoredKeyWhenHidden() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            sessionVm: makeFakeSessionVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: false, accuracy: true}),
            graphAxisSettings: ({yAxisColumnKey: "score"})
        })

        selectCategory(dialog, "Graph Lines")

        const combo = findByObjectName(dialog.contentItem, "yAxisColumnComboBox")
        verify(combo !== null, "no combo box named 'yAxisColumnComboBox' found in SettingsDialog Graph Lines category")
        compare(combo.displayText, "Accuracy", "the combo box should fall back to the first visible column")
        compare(dialog.graphAxisSettings.yAxisColumnKey, "score", "falling back should not rewrite the stored key")
    }
}
