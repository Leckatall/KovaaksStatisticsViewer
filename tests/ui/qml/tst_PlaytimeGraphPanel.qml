import QtQuick
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"

Item {
    id: root

    width: 900
    height: 400

    QtObject {
        id: fakePlaytimeVm

        property bool hasData: false
    }

    Component {
        id: panelComponent

        PlaytimeGraphPanel {
            width: 900
            height: 400
            playtimeVm: fakePlaytimeVm
        }
    }

    TestCase {
        name: "PlaytimeGraphPanelTest"
        when: windowShown

        function init() {
            fakePlaytimeVm.hasData = false
        }

        // no playtime shows a message, not an empty grid with axes.
        function test_noPlaytimeShowsAMessageAndNoAxes() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")

            const message = findChild(panel, "playtimeEmptyLabel")
            verify(!!message, "Empty-state label exists")
            compare(message.visible, true)
            verify(message.text.length > 0)
            compare(findChild(panel, "playtimeGraphCanvas"), null, "no graph canvas (and so no axes) is created")
        }

        function test_playtimeShowsTheGraphAndHidesTheMessage() {
            fakePlaytimeVm.hasData = true
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")

            tryVerify(() => findChild(panel, "playtimeGraphCanvas") !== null, 2000, "graph canvas is created")
            compare(findChild(panel, "playtimeEmptyLabel").visible, false)
        }

        function test_contentAreaKeepsItsSizeAcrossStates() {
            const panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            const content = findChild(panel, "playtimeContentArea")
            const empty = content.height
            verify(empty > 0)

            fakePlaytimeVm.hasData = true
            compare(content.height, empty)
        }
    }
}
