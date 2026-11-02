import QtQuick
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "CaptureStatusStripTest"
    width: 400
    height: 100
    when: windowShown
    visible: true

    Component { id: stubComponent; CaptureVmStub {} }
    Component { id: stripComponent; CaptureStatusStrip { width: 400; height: 26 } }

    function find(root, name) { return ItemLookup.findByObjectName(root, name) }

    function makeStrip(props) {
        const vm = createTemporaryObject(stubComponent, testCase, props || {})
        const strip = createTemporaryObject(stripComponent, testCase, {captureVm: vm})
        verify(strip !== null)
        verify(strip.captureVm === vm)
        return {vm, strip}
    }

    function test_loads() {
        const {strip} = makeStrip()
        verify(strip !== null)
    }

    function test_failureCarriesIconTextAndDismiss() {
        const {vm, strip} = makeStrip({feedbackKind: "failure", feedbackText: "Could not copy Run graph"})
        const text = find(strip, "captureStatusText")
        verify(text !== null, "the strip has no status text")
        compare(text.text, "Could not copy Run graph")
        compare(text.visible, true)
        compare(text.Accessible.role, Accessible.AlertMessage)
        const icon = find(strip, "captureStatusIcon")
        verify(icon !== null, "the strip has no status icon")
        compare(icon.visible, true)
        const dismiss = find(strip, "captureStatusDismiss")
        verify(dismiss !== null, "the strip has no dismiss button")
        verify(dismiss.Accessible.name.length > 0)
        mouseClick(dismiss)
        compare(vm.callsNamed("dismissFeedback").length, 1)
    }

    function test_successHasNoDismissNeed() {
        const failure = makeStrip({feedbackKind: "failure", feedbackText: "Could not copy"})
        const success = makeStrip({feedbackKind: "success", feedbackText: "Copied Run graph"})
        const text = find(success.strip, "captureStatusText")
        verify(text !== null, "the strip has no status text")
        compare(text.visible, true)
        const successIcon = find(success.strip, "captureStatusIcon")
        const failureIcon = find(failure.strip, "captureStatusIcon")
        verify(successIcon !== null && failureIcon !== null, "the strip has no status icon")
        verify(successIcon.source.toString() !== failureIcon.source.toString(),
               "success and failure must differ by more than colour")
    }
}
