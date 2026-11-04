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

    function test_loads() {
        const {actions} = makeActions()
        verify(actions !== null)
    }

    function test_clickingTheCameraCopiesWithoutTheMenu() {
        const {vm, canvas, actions} = makeActions()
        mouseClick(required(actions, "captureCopy_run"))
        const copies = vm.callsNamed("quickCopy")
        compare(copies.length, 1)
        compare(copies[0].args[0], "run")
        verify(copies[0].args[1] === canvas)
        compare(vm.callsNamed("openPreview").length, 0)
        compare(actions.menu.opened, false)
    }

    function test_theMenuOpensThePreview() {
        const {vm, canvas, actions} = makeActions()
        const dropdown = required(actions, "captureMenu_run")
        dropdown.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Space)
        tryVerify(() => actions.menu.opened, 2000, "the dropdown never opened")
        compare(actions.menu.itemAt(0).objectName, "captureExport_run")
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
        const opened = vm.callsNamed("openPreview")
        compare(opened.length, 1)
        compare(opened[0].args[0], "run")
        verify(opened[0].args[1] === canvas)
        compare(vm.callsNamed("quickCopy").length, 0)
    }

    function test_namesIdentifyCopyingAndTheTarget() {
        const {actions} = makeActions()
        const camera = required(actions, "captureCopy_run")
        const dropdown = required(actions, "captureMenu_run")
        for (const text of [camera.Accessible.name, camera.ToolTip.text]) {
            verify(/copy/i.test(text), "no copy verb in: " + text)
            verify(text.indexOf("Run graph") >= 0, "no target in: " + text)
        }
        verify(dropdown.Accessible.name.indexOf("Run graph") >= 0)
    }

    function test_keyboardActivatesTheCamera() {
        const {vm, actions} = makeActions()
        const camera = required(actions, "captureCopy_run")
        camera.forceActiveFocus(Qt.TabFocusReason)
        compare(camera.visualFocus, true)
        keyClick(Qt.Key_Space)
        compare(vm.callsNamed("quickCopy").length, 1)
    }

    Component { id: fieldComponent; TextField {} }

    function openPreviewFrom(actions, key) {
        required(actions, "captureMenu_" + key).forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Space)
        tryVerify(() => actions.menu.opened, 2000, "the dropdown never opened")
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
    }

    function test_focusReturnsToTheOriginOnClose() {
        const {vm, actions} = makeActions()
        const elsewhere = createTemporaryObject(fieldComponent, testCase, {y: 100})
        openPreviewFrom(actions, "run")
        compare(vm.callsNamed("openPreview").length, 1)
        elsewhere.forceActiveFocus()
        verify(elsewhere.activeFocus)
        vm.previewClosed()
        tryVerify(() => required(actions, "captureMenu_run").activeFocus, 2000,
                  "the dropdown that opened the preview did not take focus back")
        compare(required(actions, "captureCopy_run").activeFocus, false)
    }

    function test_anotherInstanceDoesNotTakeFocus() {
        const vm = createTemporaryObject(stubComponent, testCase)
        const first = createTemporaryObject(actionsComponent, testCase,
            {captureVm: vm, targetKey: "run", targetName: "Run graph"})
        const second = createTemporaryObject(actionsComponent, testCase,
            {captureVm: vm, targetKey: "dailyPlaytime", targetName: "Daily playtime", y: 50})
        const elsewhere = createTemporaryObject(fieldComponent, testCase, {y: 100})
        openPreviewFrom(first, "run")
        elsewhere.forceActiveFocus()
        vm.previewClosed()
        wait(100)
        compare(required(second, "captureCopy_dailyPlaytime").activeFocus, false)
        compare(required(second, "captureMenu_dailyPlaytime").activeFocus, false)
    }

    function test_hiddenWithoutAViewModel() {
        const {actions} = makeActions({captureVm: null})
        compare(actions.visible, false)
    }
}
