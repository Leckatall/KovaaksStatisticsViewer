import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"

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
        height: 300
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

    // One chip per row, top to bottom in the line-list order, with "Configure..." below them.
    function test_chips_stack_vertically_in_list_order() {
        const score = findChild(panel, "seriesVisibilityCheckBox_1")
        const accuracy = findChild(panel, "seriesVisibilityCheckBox_2")
        const button = findChild(panel, "configureGraphLinesButton")
        const scoreTop = score.mapToItem(panel, 0, 0).y
        const accuracyTop = accuracy.mapToItem(panel, 0, 0).y
        compare(score.mapToItem(panel, 0, 0).x, accuracy.mapToItem(panel, 0, 0).x)
        verify(accuracyTop >= scoreTop + score.height, "Accuracy sits below Score")
        verify(button.mapToItem(panel, 0, 0).y >= accuracyTop + accuracy.height, "Configure... sits below the chips")
    }

    // The legend is as wide as its widest chip.
    function test_width_is_the_widest_chip() {
        const score = findChild(panel, "seriesVisibilityCheckBox_1")
        const accuracy = findChild(panel, "seriesVisibilityCheckBox_2")
        verify(panel.implicitWidth >= Math.max(score.implicitWidth, accuracy.implicitWidth))
        verify(panel.implicitWidth <= Math.max(score.implicitWidth, accuracy.implicitWidth,
                                               findChild(panel, "configureGraphLinesButton").implicitWidth) + 1)
    }

    // The legend must never decide its panel's height: only one row is asked for.
    function test_implicit_height_ignores_the_chips() {
        const legend = createTemporaryObject(manyLinesComponent, testCase, {height: 400})
        verify(legend.implicitHeight < 2 * findChild(legend, "seriesVisibilityCheckBox_1").height)
    }

    // Too short for every chip: the chip list scrolls and "Configure..." stays inside the legend.
    function test_chips_scroll_when_the_legend_is_short() {
        const legend = createTemporaryObject(manyLinesComponent, testCase, {height: 130})
        const list = findChild(legend, "graphLineChipList")
        tryVerify(() => list.contentHeight > list.height)
        compare(findChild(legend, "graphLineChipScrollBar").policy, ScrollBar.AlwaysOn, "a scroll bar shows")
        const button = findChild(legend, "configureGraphLinesButton")
        verify(button.height > 0)
        verify(button.mapToItem(legend, 0, button.height).y <= legend.height + 0.5, "Configure... is not cut off")
    }

    function test_chips_do_not_scroll_when_they_fit() {
        const legend = createTemporaryObject(manyLinesComponent, testCase, {height: 600})
        const list = findChild(legend, "graphLineChipList")
        verify(list.contentHeight <= list.height)
        compare(findChild(legend, "graphLineChipScrollBar").policy, ScrollBar.AlwaysOff)
    }

    // A very long custom name is elided at the cap instead of widening the legend.
    function test_long_names_are_elided_at_the_cap() {
        const legend = createTemporaryObject(manyLinesComponent, testCase, {height: 600})
        const longChip = findChild(legend, "seriesVisibilityCheckBox_9")
        verify(longChip.implicitWidth > legend.maximumChipWidth)
        compare(longChip.width, legend.maximumChipWidth)
        compare(longChip.contentItem.elide, Text.ElideRight)
        verify(legend.implicitWidth <= legend.maximumChipWidth + 20)
    }

    property var manyLinesVm: QtObject {
        property var allSeries: [
            {"id": "1", "name": "Score", "color": "#009600"},
            {"id": "2", "name": "Accuracy", "color": "#00ffff"},
            {"id": "3", "name": "Shots", "color": "#ffa500"},
            {"id": "4", "name": "Hits", "color": "#6495ed"},
            {"id": "5", "name": "Kills", "color": "#ff0000"},
            {"id": "6", "name": "Dmg", "color": "#ffff00"},
            {"id": "7", "name": "Score Total", "color": "#800080"},
            {"id": "8", "name": "Expected Final Score", "color": "#ff00ff"},
            {"id": "9", "name": "A custom line with a remarkably long and descriptive name", "color": "#00bfff"}
        ]
        property var enabledSeriesIds: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]
    }

    Component {
        id: manyLinesComponent
        ControlPanel {
            graphVm: testCase.manyLinesVm
            visualSettings: testCase.visualSettings
        }
    }

    function test_configure_button_raises_the_signal() {
        const spy = createTemporaryObject(signalSpyComponent, testCase, {target: panel,
                                          signalName: "configureLinesRequested"})
        const button = findChild(panel, "configureGraphLinesButton")
        verify(button !== null)
        button.clicked()
        compare(spy.count, 1)
    }

    function test_configure_footer_shows_by_default() {
        compare(panel.showConfigureButton, true)
        testCase.visible = true
        const button = findChild(panel, "configureGraphLinesButton")
        compare(button.visible, true)
        verify(panel.implicitWidth >= button.implicitWidth)
        compare(panel.implicitHeight, button.implicitHeight)
        testCase.visible = false
    }

    function test_hiding_the_configure_footer_removes_it_and_its_space() {
        testCase.visible = true
        const legend = createTemporaryObject(manyLinesComponent, testCase, {height: 400, showConfigureButton: false})
        const button = findChild(legend, "configureGraphLinesButton")
        compare(button.visible, false)
        verify(findChild(legend, "seriesVisibilityCheckBox_1").visible, "the chips stay visible")
        compare(legend.implicitHeight, 1)
        const chip = findChild(legend, "seriesVisibilityCheckBox_1")
        const list = findChild(legend, "graphLineChipList")
        compare(list.height, legend.height)
        verify(legend.implicitWidth >= Math.min(chip.implicitWidth, legend.maximumChipWidth))
        testCase.visible = false
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
