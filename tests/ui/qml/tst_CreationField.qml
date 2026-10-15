import QtQuick
import QtTest
import KovaaksStatsViewer

TestCase {
    id: testCase
    name: "CreationFieldTest"
    width: 300
    height: 120
    visible: true
    when: windowShown

    property var submitted: []
    property var focusAtSubmit: []
    property bool accept: true

    Component {
        id: fieldComponent
        CreationField {
            width: 200
            height: 30
            submit: name => {
                testCase.submitted.push(name)
                testCase.focusAtSubmit.push(activeFocus)
                return testCase.accept
            }
        }
    }

    Component {
        id: targetComponent
        Item {
            width: 50
            height: 30
            activeFocusOnTab: true
        }
    }

    SignalSpy { id: doneSpy; signalName: "done" }

    function init() {
        submitted = []
        focusAtSubmit = []
        accept = true
        doneSpy.clear()
    }

    function makeField(properties) {
        const field = createTemporaryObject(fieldComponent, testCase, properties || {})
        verify(field)
        doneSpy.target = field
        field.forceActiveFocus()
        verify(field.activeFocus)
        return field
    }

    function test_blankInputIsIgnored() {
        const field = makeField()
        keyClick(Qt.Key_Return)
        field.text = "   "
        keyClick(Qt.Key_Return)
        compare(submitted, [])
        compare(doneSpy.count, 0)
        compare(field.text, "   ")
    }

    function test_acceptedSubmitClearsAndEmitsDone() {
        const field = makeField()
        field.text = " Gold "
        keyClick(Qt.Key_Return)
        compare(submitted, [" Gold "])
        compare(field.text, "")
        compare(doneSpy.count, 1)
    }

    function test_rejectedSubmitKeepsTextAndDoesNotEmitDone() {
        accept = false
        const field = makeField()
        field.text = "Gold"
        keyClick(Qt.Key_Return)
        compare(submitted, ["Gold"])
        compare(field.text, "Gold")
        compare(doneSpy.count, 0)
    }

    function test_escapeClearsAndEmitsDone() {
        const field = makeField()
        field.text = "Gold"
        keyClick(Qt.Key_Escape)
        compare(submitted, [])
        compare(field.text, "")
        compare(doneSpy.count, 1)
    }

    function test_focusMovesToReturnTargetBeforeSubmitAndComesBackOnRejection() {
        accept = false
        const target = createTemporaryObject(targetComponent, testCase)
        const field = makeField({focusReturnTarget: target})
        field.text = "Gold"
        keyClick(Qt.Key_Return)
        compare(submitted, ["Gold"])
        compare(focusAtSubmit, [false], "the field has let go of focus when submit runs")
        verify(field.activeFocus, "a rejected name leaves the field focused to be corrected")
        compare(field.text, "Gold")
    }

    function test_focusStaysOnReturnTargetAfterAcceptedSubmit() {
        const target = createTemporaryObject(targetComponent, testCase)
        const field = makeField({focusReturnTarget: target})
        field.text = "Gold"
        keyClick(Qt.Key_Return)
        verify(target.activeFocus)
        verify(!field.focus, "the finished field is not left as its focus scope's focus child")
        compare(doneSpy.count, 1)
    }

    function test_escapeReturnsFocusToTheReturnTarget() {
        const target = createTemporaryObject(targetComponent, testCase)
        const field = makeField({focusReturnTarget: target})
        field.text = "Gold"
        keyClick(Qt.Key_Escape)
        verify(target.activeFocus)
        verify(!field.focus)
        compare(field.text, "")
        compare(doneSpy.count, 1)
    }

    function test_withoutReturnTargetSubmitRunsWithFocusOnTheField() {
        const field = makeField()
        field.text = "Gold"
        keyClick(Qt.Key_Return)
        compare(focusAtSubmit, [true])
    }
}
