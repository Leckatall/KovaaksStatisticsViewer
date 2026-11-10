import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "CaptureActionsTest"
    width: 400
    height: 200
    when: windowShown
    visible: true

    Component { id: stubComponent; CaptureVmStub {} }
    Component { id: canvasComponent; Item {} }
    Component { id: actionsComponent; CaptureActions {} }
    Component { id: fieldComponent; TextField {} }

    function find(root, name) { return ItemLookup.findByObjectName(root, name) }

    function makeActions(props) {
        const vm = createTemporaryObject(stubComponent, testCase)
        const canvas = createTemporaryObject(canvasComponent, testCase)
        const actions = createTemporaryObject(actionsComponent, testCase, Object.assign(
            {captureVm: vm, targetKey: "run", targetName: "Run graph", canvas: canvas}, props || {}))
        verify(actions !== null)
        verify(actions.captureVm === (props && "captureVm" in props ? props.captureVm : vm))
        return {vm, canvas, actions}
    }

    function required(actions, name) {
        const item = find(actions, name)
        verify(item !== null, name + " does not exist")
        return item
    }

    function buttons(actions) {
        return ["captureCopy_run", "captureSave_run", "capturePreview_run"].map(name => required(actions, name))
    }

    function test_loads() {
        const {actions} = makeActions()
        verify(actions !== null)
    }

    function test_clickingCopyCopiesWithoutThePreview() {
        const {vm, canvas, actions} = makeActions()
        mouseClick(required(actions, "captureCopy_run"))
        const copies = vm.callsNamed("quickCopy")
        compare(copies.length, 1)
        compare(copies[0].args[0], "run")
        verify(copies[0].args[1] === canvas)
        compare(vm.callsNamed("openPreview").length, 0)
        compare(vm.callsNamed("quickSave").length, 0)
    }

    function test_clickingSaveSavesWithoutThePreview() {
        const {vm, canvas, actions} = makeActions()
        mouseClick(required(actions, "captureSave_run"))
        const saves = vm.callsNamed("quickSave")
        compare(saves.length, 1)
        compare(saves[0].args[0], "run")
        verify(saves[0].args[1] === canvas)
        compare(vm.callsNamed("openPreview").length, 0)
        compare(vm.callsNamed("quickCopy").length, 0)
    }

    function test_clickingPreviewOpensThePreview() {
        const {vm, canvas, actions} = makeActions()
        mouseClick(required(actions, "capturePreview_run"))
        const opened = vm.callsNamed("openPreview")
        compare(opened.length, 1)
        compare(opened[0].args[0], "run")
        verify(opened[0].args[1] === canvas)
        compare(vm.callsNamed("quickCopy").length, 0)
        compare(vm.callsNamed("quickSave").length, 0)
    }

    function test_namesIdentifyTheActionAndTheTarget() {
        const {actions} = makeActions()
        const [copy, save, preview] = buttons(actions)
        const verbs = [[copy, /copy/i], [save, /save/i], [preview, /preview/i]]
        for (const [button, verb] of verbs) {
            for (const text of [button.Accessible.name, button.ToolTip.text]) {
                verify(verb.test(text), "no " + verb + " in: " + text)
                verify(text.indexOf("Run graph") >= 0, "no target in: " + text)
            }
        }
        verify(actions.Accessible.name.indexOf("Run graph") >= 0,
               "toolbar name does not identify the target: " + actions.Accessible.name)
    }

    function test_theActionsAreEquallySized() {
        const {actions} = makeActions()
        const [copy, save, preview] = buttons(actions)
        for (const button of [save, preview]) {
            compare(button.width, copy.width)
            compare(button.height, copy.height)
        }
        compare(copy.width, copy.height)
    }

    function test_keyboardReachesEachActionInOrderAndActivatesIt() {
        const {vm, actions} = makeActions()
        const [copy, save, preview] = buttons(actions)
        copy.forceActiveFocus(Qt.TabFocusReason)
        compare(copy.visualFocus, true)
        keyClick(Qt.Key_Space)
        compare(vm.callsNamed("quickCopy").length, 1)
        keyClick(Qt.Key_Tab)
        verify(save.activeFocus, "Tab from Copy did not reach Save")
        compare(save.visualFocus, true)
        keyClick(Qt.Key_Space)
        compare(vm.callsNamed("quickSave").length, 1)
        keyClick(Qt.Key_Tab)
        verify(preview.activeFocus, "Tab from Save did not reach Preview")
        keyClick(Qt.Key_Space)
        compare(vm.callsNamed("openPreview").length, 1)
    }

    function test_focusReturnsToPreviewOnClose() {
        const {vm, actions} = makeActions()
        const elsewhere = createTemporaryObject(fieldComponent, testCase, {y: 100})
        const preview = required(actions, "capturePreview_run")
        preview.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Space)
        compare(vm.callsNamed("openPreview").length, 1)
        elsewhere.forceActiveFocus()
        verify(elsewhere.activeFocus)
        vm.previewClosed()
        tryVerify(() => preview.activeFocus, 2000, "the Preview button that opened the preview did not take focus back")
        compare(required(actions, "captureCopy_run").activeFocus, false)
    }

    function test_anotherInstanceDoesNotTakeFocus() {
        const vm = createTemporaryObject(stubComponent, testCase)
        const first = createTemporaryObject(actionsComponent, testCase,
            {captureVm: vm, targetKey: "run", targetName: "Run graph"})
        const second = createTemporaryObject(actionsComponent, testCase,
            {captureVm: vm, targetKey: "dailyPlaytime", targetName: "Daily playtime", y: 60})
        const elsewhere = createTemporaryObject(fieldComponent, testCase, {y: 120})
        mouseClick(required(first, "capturePreview_run"))
        elsewhere.forceActiveFocus()
        vm.previewClosed()
        wait(100)
        for (const name of ["captureCopy_dailyPlaytime", "captureSave_dailyPlaytime", "capturePreview_dailyPlaytime"])
            compare(required(second, name).activeFocus, false)
    }

    function test_hiddenWithoutAViewModel() {
        const {actions} = makeActions({captureVm: null})
        compare(actions.visible, false)
    }
}
