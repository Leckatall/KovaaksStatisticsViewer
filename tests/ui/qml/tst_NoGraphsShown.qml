import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "NoGraphsShownTest"
    when: windowShown
    width: 500
    height: 500
    visible: true

    FakeGraphVisibility { id: visibility }

    NoGraphsShown {
        id: recovery
        width: testCase.width
        message: "Your scenario graphs are hidden."
        visualSettings: visibility
    }

    function test_chooseGraphsOpensTheMenuAndItsEntriesRestoreAGraph() {
        visibility.playtimeGraphVisible = false
        mouseClick(ItemLookup.findByObjectName(recovery, "chooseGraphsButton"))
        const menu = findChild(recovery, "chooseGraphsMenu")
        tryVerify(() => menu.opened)

        let daily = null
        for (let i = 0; i < menu.count; ++i) {
            const item = menu.itemAt(i)
            if (item && item.text === "Daily playtime") daily = item
        }
        verify(daily !== null)
        compare(daily.checked, false)
        mouseClick(daily)
        compare(visibility.playtimeGraphVisible, true)
    }
}
