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
        property bool noObservationsInRange: false
        property string emptyStateText: "No playtime recorded yet."
        property bool pointMarkers: false
        property int connectionMode: 0
        property string title: "Daily Playtime (3-day rolling average)"
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
            fakePlaytimeVm.pointMarkers = false
            fakePlaytimeVm.connectionMode = 0
            fakePlaytimeVm.title = "Daily Playtime (3-day rolling average)"
        }

        function test_titleFollowsTheViewModelsEffectiveWindow() {
            const panel = createTemporaryObject(panelComponent, root)
            const title = findChild(panel, "playtimeTitleLabel")
            compare(title.text, "Daily Playtime (3-day rolling average)")

            fakePlaytimeVm.title = "Daily Playtime (daily totals)"
            compare(title.text, "Daily Playtime (daily totals)")
            fakePlaytimeVm.title = "Daily Playtime (7-day rolling average)"
            compare(title.text, "Daily Playtime (7-day rolling average)")
        }

        // The playtime graph draws with its own committed connection mode and marker preference.
        function test_canvasFollowsTheViewModelsLineStyle() {
            fakePlaytimeVm.hasData = true
            fakePlaytimeVm.pointMarkers = true
            fakePlaytimeVm.connectionMode = 1
            const panel = createTemporaryObject(panelComponent, root)
            let canvas = null
            tryVerify(() => (canvas = findChild(panel, "playtimeGraphCanvas")) !== null, 2000)
            compare(canvas.pointMarkers, true)
            compare(canvas.connectionMode, 1)

            fakePlaytimeVm.connectionMode = 2
            fakePlaytimeVm.pointMarkers = false
            compare(canvas.connectionMode, 2)
            compare(canvas.pointMarkers, false)
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
