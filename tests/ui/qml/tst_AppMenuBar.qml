import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "AppMenuBarTest"

    Component {
        id: menuBarComponent
        AppMenuBar {}
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
}
