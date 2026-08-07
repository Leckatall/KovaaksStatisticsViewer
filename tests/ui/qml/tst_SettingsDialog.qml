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
            profileDir: "file:///C:/Profile",
            profileLoaded: false
        }, overrides)
    }

    function makeFakeGraphVm() {
        return TestDoubles.makeFakeGraphVm()
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
        const dialog = createTemporaryObject(settingsDialogComponent, testCase, props)
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
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        const field = findByObjectName(dialog.contentItem, "kovaaksDirField")
        verify(field !== null, "no field named 'kovaaksDirField' found in SettingsDialog")
        compare(field.text, "D:/CustomDir", "kovaaksDirField should show settingsVm.kovaaksDir with the file:// scheme stripped")
    }

    function test_profileCategory_showsProfileDirAndLoadedState() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileDir: "file:///D:/CustomProfile", profileLoaded: true}),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        selectCategory(dialog, "Profile")

        const field = findByObjectName(dialog.contentItem, "profileDirField")
        verify(field !== null, "no field named 'profileDirField' found in SettingsDialog Profile category")
        compare(field.text, "D:/CustomProfile", "profileDirField should show settingsVm.profileDir with the file:// scheme stripped")

        const statusLabel = findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Profile category")
        compare(statusLabel.text, "Profile status: Loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === true")
    }

    function test_profileCategory_showsNotLoadedWhenProfileVmReportsFalse() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm({profileLoaded: false}),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        selectCategory(dialog, "Profile")

        const statusLabel = findByObjectName(dialog.contentItem, "profileLoadedLabel")
        verify(statusLabel !== null, "no label named 'profileLoadedLabel' found in SettingsDialog Profile category")
        compare(statusLabel.text, "Profile status: Not loaded", "profileLoadedLabel should reflect settingsVm.profileLoaded === false")
    }

    function test_columnVisibilityCheckBoxes_reflectColumnVisibility() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
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

    function test_categoryList_startsOnKovaaksDirectory() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        compare(dialog.currentCategory, 0, "SettingsDialog should default to the Kovaaks Directory category (index 0)")
    }

    function test_clickingCategoryItem_switchesCurrentCategory() {
        const dialog = openDialog({
            settingsVm: makeFakeSettingsVm(),
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({})
        })

        selectCategory(dialog, "Graph Lines")

        compare(dialog.currentCategory, 2, "clicking the 'Graph Lines' category item should switch currentCategory to index 2")
    }
}
