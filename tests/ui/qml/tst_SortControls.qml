import QtQuick
import QtTest
import "../../../src/ui/qml"

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
}
