import QtQuick
import QtTest
import "../../../src/ui/qml/scenario"

TestCase {
    id: testCase
    name: "SortControlsTest"
    when: windowShown
    width: 400
    height: 200
    visible: true

    Component {
        id: viewComponent

        SortControls {
            comboObjectName: "sortCombo"
            buttonObjectName: "sortDirectionButton"
            options: ["Date", "Score", "Accuracy"]
        }
    }

    SignalSpy {
        id: sortRequestedSpy
        signalName: "sortRequested"
    }

    function createView(props) {
        const view = createTemporaryObject(viewComponent, testCase, props)
        verify(waitForRendering(view))
        return view
    }

    function test_comboActivationEmitsRequestedFieldAndCurrentDirection() {
        const view = createView({sortAscending: true})
        const combo = findChild(view, "sortCombo")
        verify(combo !== null)
        combo.forceActiveFocus()

        const captured = []
        const onSortRequested = (field, ascending) => captured.push([field, ascending])
        view.sortRequested.connect(onSortRequested)

        keyClick(Qt.Key_Down)
        wait(50)
        view.sortRequested.disconnect(onSortRequested)

        verify(captured.length >= 1)
        const lastArgs = captured[captured.length - 1]
        compare(lastArgs[0], 1)
        compare(lastArgs[1], true)
    }

    function test_directionButtonEmitsCurrentFieldAndFlippedDirection() {
        const view = createView({sortField: 2})
        const button = findChild(view, "sortDirectionButton")
        verify(button !== null)

        sortRequestedSpy.target = view
        verify(sortRequestedSpy.valid)
        mouseClick(button, button.width / 2, button.height / 2)
        wait(0)

        compare(sortRequestedSpy.count, 1)
        compare(sortRequestedSpy.signalArguments[0][0], 2)
        compare(sortRequestedSpy.signalArguments[0][1], true)
    }
    // --- the direction toggle explains itself ---------------------------

    function test_directionButtonShowsItsStateAndFlipsItsAppearance() {
        const view = createView({sortAscending: false})
        const button = findChild(view, "sortDirectionButton")
        const descendingText = button.text
        verify(descendingText.indexOf("Desc") !== -1, "descending must be named: " + descendingText)
        verify(button.Accessible.name.indexOf("descending") !== -1, button.Accessible.name)
        const width = button.width

        view.sortAscending = true
        wait(0)
        verify(button.text !== descendingText, "the button must look different once the direction flips")
        verify(button.text.indexOf("Asc") !== -1, button.text)
        verify(button.Accessible.name.indexOf("ascending") !== -1, button.Accessible.name)
        compare(button.width, width, "flipping must not resize the button and shift the row")
    }

    function test_directionButtonHasATooltipOnHover() {
        const view = createView({})
        const button = findChild(view, "sortDirectionButton")
        verify(view.directionToolTip.indexOf("descending") !== -1)
        verify(!view.directionToolTipShown)
        // No hoverEnabled workaround: the tooltip must work where the platform turns
        // hover effects off (the offscreen test platform).
        mouseMove(button, button.width / 2, button.height / 2)
        tryVerify(() => view.directionToolTipShown, 2000, "hovering the direction toggle must show a tooltip")
        compare(button.Accessible.description, view.directionToolTip)
    }

    function test_sortByLabelSitsOnTheComboRow() {
        const view = createView({})
        const label = findChild(view, "sortByLabel")
        const combo = findChild(view, "sortCombo")
        verify(label !== null && label.visible)
        verify(label.x + label.width <= combo.x, "the label is to the left of its combo box")
        const labelMid = label.y + label.height / 2
        verify(labelMid > combo.y && labelMid < combo.y + combo.height, "the label is vertically centred on the combo row")
    }
}
