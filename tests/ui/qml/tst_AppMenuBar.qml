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
        verify(menuBar !== null)

        let emitted = 0
        menuBar.setSourceDirRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "Set Source Directory")
        verify(item !== null)
        item.action.trigger()

        compare(emitted, 1)
    }

    function test_settingsRequested_emittedWhenActionTriggered() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null)

        let emitted = 0
        menuBar.settingsRequested.connect(() => emitted++)

        const item = findMenuItemByText(menuBar.menuAt(0), "Settings")
        verify(item !== null)
        item.action.trigger()

        compare(emitted, 1)
    }

    function test_unrelatedActions_doNotEmitEitherSignal() {
        const menuBar = createTemporaryObject(menuBarComponent, testCase)
        verify(menuBar !== null)

        let sourceDirEmitted = 0
        let settingsEmitted = 0
        menuBar.setSourceDirRequested.connect(() => sourceDirEmitted++)
        menuBar.settingsRequested.connect(() => settingsEmitted++)

        const quitItem = findMenuItemByText(menuBar.menuAt(0), "&Quit")
        verify(quitItem !== null)
        quitItem.action.trigger()

        compare(sourceDirEmitted, 0)
        compare(settingsEmitted, 0)
    }
}
