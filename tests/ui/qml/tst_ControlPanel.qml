import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "ControlPanelTest"
    when: windowShown

    property var fakeGraphVm: QtObject {
        property var allSeries: [
            {"id": "1", "name": "Score", "color": "#009600"},
            {"id": "2", "name": "Accuracy", "color": "#00ffff"}
        ]
        property var enabledSeriesIds: ["1", "2"]
    }
    property var visualSettings: QtObject {
        property var visibleSeriesIds: []
        function isSeriesVisible(id) { return visibleSeriesIds.indexOf(id) !== -1 }
        function setSeriesVisible(id, visible) {
            const index = visibleSeriesIds.indexOf(id)
            if (visible && index === -1) visibleSeriesIds = visibleSeriesIds.concat([id])
            else if (!visible && index !== -1) {
                const next = visibleSeriesIds.slice()
                next.splice(index, 1)
                visibleSeriesIds = next
            }
        }
        function syncVisibleSeriesIds(ids) { visibleSeriesIds = ids }
    }

    Component.onCompleted: visualSettings.syncVisibleSeriesIds(fakeGraphVm.enabledSeriesIds)

    ControlPanel {
        id: panel
        graphVm: testCase.fakeGraphVm
        visualSettings: testCase.visualSettings
    }

    function test_checkbox_starts_checked_for_visible_series() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_1")
        verify(checkbox !== null)
        compare(checkbox.checked, true)
    }

    function test_unchecking_then_rechecking_round_trips_visibility() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_1")
        checkbox.checked = false
        checkbox.toggled()
        compare(testCase.visualSettings.isSeriesVisible("1"), false)
        checkbox.checked = true
        checkbox.toggled()
        compare(testCase.visualSettings.isSeriesVisible("1"), true)
    }

    // the swatch is the line's exact colour at full opacity, not a translucent tint.
    function test_swatch_matches_the_line_colour_exactly() {
        const swatch = findChild(panel, "seriesSwatch_1")
        verify(swatch !== null)
        compare(swatch.opacity, 1)
        verify(Qt.colorEqual(swatch.color, "#009600"), "swatch fill " + swatch.color)
        verify(Qt.colorEqual(swatch.border.color, "#009600"))

        const accuracy = findChild(panel, "seriesSwatch_2")
        verify(Qt.colorEqual(accuracy.color, "#00ffff"), "swatch fill " + accuracy.color)
    }

    function test_hidden_line_keeps_its_colour_as_an_outline() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_2")
        const swatch = findChild(panel, "seriesSwatch_2")
        checkbox.checked = false
        checkbox.toggled()
        verify(Qt.colorEqual(swatch.color, "transparent"))
        verify(Qt.colorEqual(swatch.border.color, "#00ffff"))
        checkbox.checked = true
        checkbox.toggled()
        verify(Qt.colorEqual(swatch.color, "#00ffff"))
    }

    // The label is drawn in the normal text colour, not on a series-coloured background.
    function test_label_uses_the_text_colour() {
        const checkbox = findChild(panel, "seriesVisibilityCheckBox_1")
        verify(Qt.colorEqual(checkbox.contentItem.color, checkbox.palette.text))
        verify(checkbox.background.color.a < 0.2, "no series-coloured chip background")
    }

    function test_configure_button_raises_the_signal() {
        const spy = createTemporaryObject(signalSpyComponent, testCase, {target: panel,
                                          signalName: "configureLinesRequested"})
        const button = findChild(panel, "configureGraphLinesButton")
        verify(button !== null)
        button.clicked()
        compare(spy.count, 1)
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
