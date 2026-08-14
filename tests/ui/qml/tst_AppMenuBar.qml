import QtQuick
import QtTest
import "../../../src/ui/qml"
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "AppMenuBarTest"

    Component {
        id: menuBarComponent
        AppMenuBar {}
    }

    QtObject {
        id: fakeViewSettings
        property bool scenarioGraphVisible: true
        property bool playtimeGraphVisible: true
        property bool scenarioHistoryGraphVisible: true
        property bool controlPanelVisible: true
        property bool selectionPanelVisible: true
        property bool recentRunsSectionVisible: true
        property bool scenarioBrowserSectionVisible: true
    }

    QtObject {
        id: fakeColumnVisibility
        property bool score: true
        property bool accuracy: true
    }

    QtObject {
        id: fakeHistoryColumnVisibility
        property bool score: true
        property bool accuracy: false
        property bool shots: false
        property bool hits: false
        property bool misses: false
    }

    readonly property var fakeGraphVm: TestDoubles.makeFakeGraphVm()
    readonly property var fakeHistoryVm: TestDoubles.makeFakeHistoryVm()

    Component {
        id: wiredMenuBarComponent
        AppMenuBar {
            graphVm: fakeGraphVm
            historyVm: fakeHistoryVm
            columnVisibility: fakeColumnVisibility
            historyColumnVisibility: fakeHistoryColumnVisibility
            viewSettings: fakeViewSettings
        }
    }


    // Menu items created from a bare `Action { ... }` child are wrapped by
    // Menu into a MenuItem exposing `.action`; find them by label instead of
    // index so the tests survive reordering menu entries.
    function findMenuItemByText(menu, text) {
        for (let i = 0; i < menu.count; ++i) {
            const item = menu.itemAt(i)
            if (item && item.text === text) return item
        }
        return null
    }

    function test_setSourceDirRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let emitted = 0
        menuBar.setSourceDirRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "Set Source &Directory")
        verify(item !== null, "no menu item with text 'Set Source Directory' found in menuAt(0)")
        item.action.trigger()

        compare(emitted, 1, "setSourceDirRequested should fire exactly once when its action is triggered")
    }

    function test_settingsRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let emitted = 0
        menuBar.settingsRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "&Settings")
        verify(item !== null, "no menu item with text 'Settings' found in menuAt(0)")
        item.action.trigger()

        compare(emitted, 1, "settingsRequested should fire exactly once when its action is triggered")
    }

    function test_quitRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let emitted = 0
        menuBar.quitRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "&Quit")
        verify(item !== null, "no menu item with text '&Quit' found in menuAt(0)")
        item.action.trigger()

        compare(emitted, 1, "quitRequested should fire exactly once when its action is triggered")
    }

    function test_aboutRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let emitted = 0
        menuBar.aboutRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(2), "&About")
        verify(item !== null, "no menu item with text '&About' found in menuAt(2)")
        item.action.trigger()

        compare(emitted, 1, "aboutRequested should fire exactly once when its action is triggered")
    }

    function test_unrelatedActions_doNotEmitEitherSignal() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let sourceDirEmitted = 0
        let settingsEmitted = 0
        menuBar.setSourceDirRequested.connect(() => sourceDirEmitted++)
        menuBar.settingsRequested.connect(() => settingsEmitted++)

        const quitItem = findMenuItemByText(menuBar.menuAt(0), "&Quit")
        verify(quitItem !== null, "no menu item with text '&Quit' found in menuAt(0)")
        quitItem.action.trigger()

        compare(sourceDirEmitted, 0, "triggering Quit should not emit setSourceDirRequested")
        compare(settingsEmitted, 0, "triggering Quit should not emit settingsRequested")
    }

    function test_loadPerformanceFileRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")

        let emitted = 0
        menuBar.loadPerformanceFileRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "&Load Performance File...")
        verify(item !== null, "no menu item with text 'Load Performance File...' found in menuAt(0)")
        item.action.trigger()

        compare(emitted, 1, "loadPerformanceFileRequested should fire exactly once when its action is triggered")
    }

    function test_masterToggleActions_readAndWriteViewSettings() {
        fakeViewSettings.scenarioGraphVisible = true
        fakeViewSettings.playtimeGraphVisible = true
        fakeViewSettings.scenarioHistoryGraphVisible = true
        fakeViewSettings.controlPanelVisible = true
        fakeViewSettings.selectionPanelVisible = true
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        verify(menuBar !== null, "AppMenuBar failed to instantiate")
        const viewMenu = menuBar.menuAt(1)

        const scenarioGraphItem = findMenuItemByText(viewMenu, "Scenario Graph")
        verify(scenarioGraphItem !== null)
        compare(scenarioGraphItem.action.checked, true)
        scenarioGraphItem.action.trigger()
        compare(fakeViewSettings.scenarioGraphVisible, false, "triggering the checkable action should flip viewSettings")

        const playtimeItem = findMenuItemByText(viewMenu, "Playtime Graph")
        verify(playtimeItem !== null)
        playtimeItem.action.trigger()
        compare(fakeViewSettings.playtimeGraphVisible, false)

        const historyItem = findMenuItemByText(viewMenu, "Scenario History")
        verify(historyItem !== null)
        historyItem.action.trigger()
        compare(fakeViewSettings.scenarioHistoryGraphVisible, false)

        const controlPanelItem = findMenuItemByText(viewMenu, "Control Panel")
        verify(controlPanelItem !== null)
        controlPanelItem.action.trigger()
        compare(fakeViewSettings.controlPanelVisible, false)

        const selectionPanelItem = findMenuItemByText(viewMenu, "Selection Panel")
        verify(selectionPanelItem !== null)
        selectionPanelItem.action.trigger()
        compare(fakeViewSettings.selectionPanelVisible, false)
    }

    function test_scenarioGraphLinesSubmenu_enabledTracksMasterToggle() {
        fakeViewSettings.scenarioGraphVisible = true
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const linesMenu = findChild(menuBar, "scenarioGraphLinesMenu")
        verify(linesMenu !== null, "scenarioGraphLinesMenu not found")
        compare(linesMenu.enabled, true)

        fakeViewSettings.scenarioGraphVisible = false
        compare(linesMenu.enabled, false)
    }

    function test_selectionPanelSectionsSubmenu_enabledTracksMasterToggle() {
        fakeViewSettings.selectionPanelVisible = true
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const sectionsMenu = findChild(menuBar, "selectionPanelSectionsMenu")
        verify(sectionsMenu !== null, "selectionPanelSectionsMenu not found")
        compare(sectionsMenu.enabled, true)

        fakeViewSettings.selectionPanelVisible = false
        compare(sectionsMenu.enabled, false)
    }

    function test_scenarioHistoryLinesSubmenuTracksVisibilityAndColumnSettings() {
        fakeViewSettings.scenarioHistoryGraphVisible = true
        fakeHistoryColumnVisibility.score = true
        fakeHistoryColumnVisibility.accuracy = false
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const linesMenu = findChild(menuBar, "scenarioHistoryLinesMenu")
        verify(linesMenu !== null)
        compare(linesMenu.enabled, true)

        const scoreItem = findMenuItemByText(linesMenu, "Score")
        const accuracyItem = findMenuItemByText(linesMenu, "Accuracy")
        verify(scoreItem !== null)
        verify(accuracyItem !== null)
        compare(scoreItem.checked, true)
        compare(accuracyItem.checked, false)

        accuracyItem.checked = true
        accuracyItem.triggered()
        compare(fakeHistoryColumnVisibility.accuracy, true)

        fakeViewSettings.scenarioHistoryGraphVisible = false
        compare(linesMenu.enabled, false)
    }

    function test_selectionPanelSectionItems_reflectViewSettings() {
        fakeViewSettings.recentRunsSectionVisible = true
        fakeViewSettings.scenarioBrowserSectionVisible = false
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const sectionsMenu = findChild(menuBar, "selectionPanelSectionsMenu")

        const recentItem = findMenuItemByText(sectionsMenu, "Recent Runs")
        const browserItem = findMenuItemByText(sectionsMenu, "Scenario Browser")
        verify(recentItem !== null)
        verify(browserItem !== null)
        compare(recentItem.checked, true)
        compare(browserItem.checked, false)

        fakeViewSettings.scenarioBrowserSectionVisible = true
        compare(browserItem.checked, true)
    }

    function test_graphLinesSubmenu_buildsOneItemPerEnabledColumnFromGraphVm() {
        fakeColumnVisibility.score = true
        fakeColumnVisibility.accuracy = false
        fakeGraphVm.enabledColumns = [1]
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const linesMenu = findChild(menuBar, "scenarioGraphLinesMenu")

        const scoreItem = findMenuItemByText(linesMenu, "Score")
        const accuracyItem = findMenuItemByText(linesMenu, "Accuracy")
        verify(scoreItem !== null, "expected a menu item labeled 'Score' from graphVm.columnName")
        verify(accuracyItem === null)
        compare(scoreItem.checked, true)

        fakeColumnVisibility.score = false
        compare(scoreItem.checked, false, "menu item should track columnVisibility[graphVm.columnKey(col)]")
        fakeGraphVm.enabledColumns = [1, 2]
    }

    function test_graphLinesSubmenuRetainsConfigureItemWhenAllColumnsDisabled() {
        fakeGraphVm.enabledColumns = []
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const linesMenu = findChild(menuBar, "scenarioGraphLinesMenu")

        verify(findMenuItemByText(linesMenu, "Configure Lines...") !== null)
        fakeGraphVm.enabledColumns = [1, 2]
    }

    function test_configureGraphLinesRequestedEmittedFromMenuItem() {
        const menuBar = createTemporaryObject(wiredMenuBarComponent, testCase)
        const linesMenu = findChild(menuBar, "scenarioGraphLinesMenu")
        const item = findMenuItemByText(linesMenu, "Configure Lines...")
        let emitted = 0
        menuBar.configureGraphLinesRequested.connect(() => emitted++)

        verify(item !== null)
        item.action.trigger()

        compare(emitted, 1)
    }
}
