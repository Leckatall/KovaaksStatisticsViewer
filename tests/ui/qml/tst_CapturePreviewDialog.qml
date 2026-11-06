import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "CapturePreviewDialogTest"
    width: 400
    height: 300
    when: windowShown
    visible: true

    Component { id: stubComponent; CaptureVmStub {} }
    Component { id: dialogComponent; CapturePreviewDialog {} }
    Component { id: swatchComponent; Rectangle { color: "steelblue" } }

    function find(dialog, name) {
        for (const root of [dialog.contentItem, dialog.footer]) {
            const found = root ? ItemLookup.findByObjectName(root, name) : null
            if (found) return found
        }
        return null
    }

    function required(dialog, name) {
        const item = find(dialog, name)
        verify(item !== null, name + " does not exist")
        return item
    }

    function tallImage(width, height) {
        const swatch = createTemporaryObject(swatchComponent, testCase, {width: width, height: height})
        const image = grabImage(swatch)
        verify(image.width === width)
        return image
    }

    function make(props, dialogProps) {
        const vm = createTemporaryObject(stubComponent, testCase, Object.assign(
            {previewOpen: true, previewState: "ready", previewTargetName: "Run graph", canOutput: true,
             outputWidth: 1920, outputHeight: 1200}, props || {}))
        const dialog = createTemporaryObject(dialogComponent, testCase, Object.assign({captureVm: vm}, dialogProps || {}))
        verify(dialog !== null)
        verify(dialog.captureVm === vm)
        return {vm, dialog}
    }

    function shown(props, dialogProps) {
        const made = make(props, dialogProps)
        for (let i = 0; i < 50 && !made.dialog.visible; ++i) wait(20)
        verify(made.dialog.visible, "the dialog never became visible")
        verify(waitForRendering(made.dialog.contentItem))
        return made
    }

    function test_loads() {
        const {dialog} = make({previewOpen: false})
        compare(dialog.visible, false)
    }

    function test_followsPreviewOpen() {
        const {vm, dialog} = make({previewOpen: false})
        compare(dialog.visible, false)
        vm.previewOpen = true
        tryCompare(dialog, "visible", true)
        compare(dialog.modality, Qt.WindowModal)
        verify(dialog.title.indexOf("Run graph") >= 0, "title: " + dialog.title)
        vm.previewOpen = false
        tryCompare(dialog, "visible", false)
    }

    function test_readyShowsDimensionsAndEnablesOutput() {
        const {dialog} = shown()
        const size = required(dialog, "captureOutputSize")
        verify(size.text.indexOf("1920") >= 0 && size.text.indexOf("1200") >= 0, size.text)
        compare(required(dialog, "captureCopyButton").enabled, true)
        compare(required(dialog, "captureExportButton").enabled, true)
    }

    function test_renderingKeepsTheImageButDisablesOutput() {
        const {dialog} = shown({previewState: "rendering", canOutput: false, previewImage: tallImage(40, 20)})
        compare(required(dialog, "captureImage").visible, true)
        compare(required(dialog, "captureCopyButton").enabled, false)
        compare(required(dialog, "captureExportButton").enabled, false)
    }

    function test_invalidExplainsAndHidesTheImage() {
        const {dialog} = shown({previewState: "invalid", canOutput: false, previewError: "Too wide to render"})
        const image = find(dialog, "captureImage")
        verify(image === null || !image.visible, "the image must be hidden")
        const error = required(dialog, "captureError")
        compare(error.visible, true)
        compare(error.text, "Too wide to render")
        compare(required(dialog, "captureErrorIcon").visible, true)
        compare(required(dialog, "captureCopyButton").enabled, false)
        compare(required(dialog, "captureExportButton").enabled, false)
        compare(required(dialog, "captureOptions").enabled, true)
    }

    function test_failedOffersRetry() {
        const {vm, dialog} = shown({previewState: "failed", canOutput: false, previewError: "Rendering failed"})
        mouseClick(required(dialog, "captureRetry"))
        compare(vm.callsNamed("retryRender").length, 1)
    }

    function test_feedbackIsMirroredInTheFooter() {
        const {vm, dialog} = shown({feedbackKind: "failure", feedbackText: "Could not copy Run graph"})
        compare(required(dialog, "captureStatusText").text, "Could not copy Run graph")
        mouseClick(required(dialog, "captureStatusDismiss"))
        compare(vm.callsNamed("dismissFeedback").length, 1)
    }

    function assertUsableWhenConstrained(vm, dialog, minImageHeight) {
        dialog.width = dialog.minimumWidth
        dialog.height = dialog.minimumHeight
        wait(50)
        mouseClick(required(dialog, "captureAdvanced"))
        wait(50)

        for (const name of ["captureCloseButton", "captureCopyButton", "captureExportButton"]) {
            const button = required(dialog, name)
            const origin = button.mapToItem(null, 0, 0)
            verify(origin.x >= 0 && origin.y >= 0
                   && origin.x + button.width <= dialog.width
                   && origin.y + button.height <= dialog.height,
                   name + " lies outside the window")
        }
        const scroller = required(dialog, "captureImageScroll")
        verify(scroller.height >= minImageHeight, "the image area shrank to " + scroller.height)

        const options = required(dialog, "captureOptionsScroll")
        options.contentY = Math.max(0, options.contentHeight - options.height)
        const field = required(dialog, "captureKeepAspect")
        const top = field.mapToItem(options, 0, 0).y
        verify(top >= 0 && top + field.height <= options.height + 1,
               "the last option is unreachable (y=" + top + ", viewport=" + options.height + ")")

        mouseClick(required(dialog, "captureCloseButton"))
        compare(vm.callsNamed("closePreview").length, 1)
    }

    function constrainedProps() {
        return {previewImage: tallImage(200, 3000), previewState: "invalid", canOutput: false,
                previewError: "Too wide to render at this resolution, so choose a smaller resolution or size",
                widthError: "Too wide to render", heightError: "Too tall to render", isGraph: true}
    }

    function test_minimumSizeWithAdvancedKeepsImageFooterAndOptionsUsable() {
        const {vm, dialog} = shown(constrainedProps())
        assertUsableWhenConstrained(vm, dialog, 100)
    }

    function test_tabbingToAClippedOptionScrollsItIntoView() {
        const {dialog} = shown({isGraph: true, hasReference: true})
        dialog.width = dialog.minimumWidth
        dialog.height = dialog.minimumHeight
        mouseClick(required(dialog, "captureAdvanced"))
        wait(50)
        const options = required(dialog, "captureOptionsScroll")
        verify(options.contentHeight > options.height, "the options must overflow for this test to mean anything")

        required(dialog, "captureResolution").forceActiveFocus(Qt.TabFocusReason)
        const pending = ["captureKeepAspect", "captureWidth", "captureHeight"]
        for (let i = 0; i < 12 && pending.length > 0; ++i) {
            keyClick(Qt.Key_Tab)
            const focused = dialog.activeFocusItem
            const index = focused ? pending.indexOf(focused.objectName) : -1
            if (index < 0) continue
            const top = focused.mapToItem(options, 0, 0).y
            verify(top >= 0 && top + focused.height <= options.height,
                   focused.objectName + " is focused but outside the visible options (y=" + top
                   + ", height=" + focused.height + ", viewport=" + options.height + ")")
            pending.splice(index, 1)
        }
        compare(pending, [], "Tab never reached these options")
    }

    function test_largerFontKeepsImageFooterAndOptionsUsable() {
        const {vm, dialog} = shown(constrainedProps(),
                                   {font: Qt.font({pixelSize: 2 * Qt.application.font.pixelSize})})
        assertUsableWhenConstrained(vm, dialog, 100)
    }

    function test_footerStaysReachableAndTheImageScrollsAtConstrainedSizes() {
        const {dialog} = shown({previewImage: tallImage(200, 3000)},
                               {font: Qt.font({pixelSize: 2 * Qt.application.font.pixelSize})})
        dialog.width = dialog.minimumWidth
        dialog.height = dialog.minimumHeight
        wait(50)
        const scroller = required(dialog, "captureImageScroll")
        scroller.forceActiveFocus()
        compare(scroller.contentY, 0)
        keyClick(Qt.Key_PageDown)
        verify(scroller.contentY > 0, "PageDown did not scroll the image")
    }

    function test_resolutionOffersTheMarkedRange() {
        const {vm, dialog} = shown({percent: 200})
        const slider = required(dialog, "captureResolution")
        compare(slider.from, 50)
        compare(slider.to, 500)
        for (const mark of [50, 100, 200, 300, 400, 500]) {
            const label = required(dialog, "captureMark_" + mark)
            compare(label.visible, true)
            verify(label.text.indexOf(String(mark)) >= 0, label.text)
        }
        slider.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Right)
        const calls = vm.callsNamed("setPercent")
        verify(calls.length >= 1, "moving the slider did not call setPercent")
        verify(calls[calls.length - 1].args[0] >= 50 && calls[calls.length - 1].args[0] <= 500)
    }

    function test_summaryOptionFollowsAvailability() {
        const table = shown({isGraph: false, summaryAvailable: true})
        const box = required(table.dialog, "captureSummary")
        compare(box.checked, true)
        mouseClick(box)
        const calls = table.vm.callsNamed("setIncludeSummary")
        compare(calls.length, 1)
        compare(calls[0].args[0], false)

        table.vm.summaryAvailable = false
        compare(box.enabled, false)

        const graph = shown({isGraph: true})
        const none = find(graph.dialog, "captureSummary")
        verify(none === null || !none.visible, "a graph offers no summary")
    }

    function test_advancedIsCollapsedAndGraphOnly() {
        const graph = shown({isGraph: true})
        const hidden = find(graph.dialog, "captureWidth")
        verify(hidden === null || !hidden.visible, "the width field must start collapsed")
        mouseClick(required(graph.dialog, "captureAdvanced"))
        compare(required(graph.dialog, "captureWidth").visible, true)
        compare(required(graph.dialog, "captureHeight").visible, true)

        const table = shown({isGraph: false, summaryAvailable: true})
        compare(find(table.dialog, "captureAdvanced"), null)
        compare(find(table.dialog, "captureWidth"), null)
        compare(find(table.dialog, "captureHeight"), null)
    }

    function test_advancedFieldsShareOneRowEvenWithErrors() {
        const {dialog} = shown({isGraph: true, widthError: "Too wide to render", heightError: "Too tall to render"})
        mouseClick(required(dialog, "captureAdvanced"))
        const width = required(dialog, "captureWidth")
        const height = required(dialog, "captureHeight")
        const at = item => item.mapToItem(null, 0, 0)
        compare(at(height).y, at(width).y)
        verify(at(height).x > at(width).x + width.width, "the height field must sit right of the width field")
    }

    function test_committingAFieldKeepsItsErrorShown() {
        const {vm, dialog} = shown({isGraph: true, widthText: "99999", widthError: "Too wide to render"})
        mouseClick(required(dialog, "captureAdvanced"))
        const field = required(dialog, "captureWidth")
        field.forceActiveFocus()
        field.selectAll()
        for (const character of "abc") keyClick(character)
        keyClick(Qt.Key_Return)
        const commits = vm.callsNamed("commitWidth")
        verify(commits.length >= 1, "Enter did not commit the width")
        compare(commits[commits.length - 1].args[0], "abc")
        const error = required(dialog, "captureWidthError")
        compare(error.visible, true)
        compare(error.text, "Too wide to render")
        verify(field.Accessible.description.indexOf("Too wide to render") >= 0)
        compare(field.text, "abc")
    }

    function test_lockedChoicesCannotChange() {
        const graph = shown({isGraph: true, optionsLocked: true})
        compare(required(graph.dialog, "captureResolution").enabled, false)
        mouseClick(required(graph.dialog, "captureAdvanced"))
        compare(required(graph.dialog, "captureWidth").enabled, false)
        compare(required(graph.dialog, "captureHeight").enabled, false)
        compare(required(graph.dialog, "captureKeepAspect").enabled, false)

        const table = shown({isGraph: false, summaryAvailable: true, optionsLocked: true})
        compare(required(table.dialog, "captureSummary").enabled, false)
    }

    function saveDialogOf(dialog) {
        return findChild(dialog, "captureSaveDialog") || findChild(dialog.contentItem, "captureSaveDialog")
    }

    function outputCalls(vm) {
        return vm.callsNamed("copyPreview").length + vm.callsNamed("confirmExport").length
    }

    function test_copyIsIconOnlyAndNamed() {
        const {vm, dialog} = shown()
        const copy = required(dialog, "captureCopyButton")
        compare(copy.Accessible.name, "Copy image to clipboard")
        compare(copy.ToolTip.text, "Copy image to clipboard")
        compare(copy.display, AbstractButton.IconOnly)
        mouseClick(copy)
        compare(vm.callsNamed("copyPreview").length, 1)
        compare(dialog.visible, true)
    }

    function test_closeEscapeAndWindowCloseStartNothing() {
        const {vm, dialog} = shown()
        mouseClick(required(dialog, "captureCloseButton"))
        compare(vm.callsNamed("closePreview").length, 1)

        required(dialog, "captureImageScroll").forceActiveFocus()
        keyClick(Qt.Key_Escape)
        compare(vm.callsNamed("closePreview").length, 2)

        dialog.close()
        compare(vm.callsNamed("closePreview").length, 3)
        compare(outputCalls(vm), 0)
        compare(vm.callsNamed("beginExport").length, 0)
    }

    // Quitting sends a close to every window the app has shown, hidden ones too; refusing it
    // cancels the quit and leaves the process running with no window.
    function test_aHiddenPreviewDoesNotRefuseClosing() {
        const {vm, dialog} = shown()
        vm.previewOpen = false
        tryCompare(dialog, "visible", false)

        let accepted = null
        const record = close => { accepted = close.accepted }
        dialog.closing.connect(record)
        dialog.close()
        dialog.closing.disconnect(record)
        compare(accepted, true)
        compare(vm.callsNamed("closePreview").length, 0)
    }

    function test_escapeClosesAfterCopyWithoutForcingFocus() {
        const {vm, dialog} = shown()
        mouseClick(required(dialog, "captureCopyButton"))
        compare(vm.callsNamed("copyPreview").length, 1)
        keyClick(Qt.Key_Escape)
        compare(vm.callsNamed("closePreview").length, 1)
    }

    function test_escapeClosesFromEachFooterButtonReachedByTab() {
        const {vm, dialog} = shown()
        let reached = []
        for (let i = 0; i < 30 && reached.length < 3; ++i) {
            keyClick(Qt.Key_Tab)
            const name = dialog.activeFocusItem ? dialog.activeFocusItem.objectName : ""
            if (/^capture(Close|Copy|Export)Button$/.test(name) && !reached.includes(name)) {
                reached.push(name)
                keyClick(Qt.Key_Escape)
            }
        }
        compare(reached.length, 3)
        compare(vm.callsNamed("closePreview").length, 3)
    }

    function test_sizeFieldsWaitForAReferenceSize() {
        const {vm, dialog} = shown({isGraph: true, hasReference: false})
        mouseClick(required(dialog, "captureAdvanced"))
        compare(required(dialog, "captureWidth").enabled, false)
        compare(required(dialog, "captureHeight").enabled, false)
        vm.hasReference = true
        compare(required(dialog, "captureWidth").enabled, true)
        compare(required(dialog, "captureHeight").enabled, true)
    }

    function test_exportOpensThePickerOnlyWhenAdmitted() {
        const {vm, dialog} = shown({beginExportResult: false})
        const button = required(dialog, "captureExportButton")
        mouseClick(button)
        compare(vm.callsNamed("beginExport").length, 1)
        compare(dialog.savePickerOpen, false)

        vm.beginExportResult = true
        mouseClick(button)
        compare(dialog.savePickerOpen, true)
        const picker = saveDialogOf(dialog)
        verify(picker !== null, "captureSaveDialog does not exist")
        compare(picker.fileMode, FileDialog.SaveFile)
        verify(picker.nameFilters.some(filter => /png/i.test(filter)), "no PNG filter")
        verify(picker.currentFile.toString().endsWith("capture.png"), picker.currentFile.toString())
        picker.close()
    }

    function test_cancellingThePickerReturnsToThePreview() {
        const {vm, dialog} = shown()
        mouseClick(required(dialog, "captureExportButton"))
        const picker = saveDialogOf(dialog)
        verify(picker !== null, "captureSaveDialog does not exist")
        picker.rejected()
        compare(vm.callsNamed("cancelExport").length, 1)
        compare(dialog.savePickerOpen, false)
        compare(dialog.visible, true)
    }

    function test_confirmingHandsOverTheChosenFile() {
        const {vm, dialog} = shown()
        mouseClick(required(dialog, "captureExportButton"))
        const picker = saveDialogOf(dialog)
        verify(picker !== null, "captureSaveDialog does not exist")
        picker.accept()
        const confirms = vm.callsNamed("confirmExport")
        compare(confirms.length, 1)
        verify(picker.selectedFile.toString() !== "", "the picker selected nothing")
        compare(confirms[0].args[0].toString(), picker.selectedFile.toString())
        compare(vm.callsNamed("copyPreview").length, 0)
        compare(vm.callsNamed("cancelExport").length, 0)
    }

    function test_advancedCollapsesOnReopen() {
        const {vm, dialog} = shown({isGraph: true})
        mouseClick(required(dialog, "captureAdvanced"))
        compare(required(dialog, "captureWidth").visible, true)
        vm.previewOpen = false
        tryCompare(dialog, "visible", false)
        vm.previewOpen = true
        tryCompare(dialog, "visible", true)
        const width = find(dialog, "captureWidth")
        verify(width === null || !width.visible, "Advanced must start collapsed on reopen")
    }

    function test_retryIsOfferedOnlyAfterAFailure() {
        const {dialog} = shown({previewState: "invalid", canOutput: false, previewError: "Too wide to render"})
        compare(required(dialog, "captureRetry").visible, false)
    }

    function test_marksSitOverTheHandlePositionAtTheEnds() {
        for (const percent of [50, 500]) {
            const {dialog} = shown({percent: percent})
            const slider = required(dialog, "captureResolution")
            const mark = required(dialog, "captureMark_" + percent)
            const handleCentre = slider.handle.mapToItem(null, slider.handle.width / 2, 0).x
            const markCentre = mark.mapToItem(null, mark.width / 2, 0).x
            verify(Math.abs(handleCentre - markCentre) < 1.5,
                   "mark " + percent + " is at " + markCentre + " but the handle is at " + handleCentre)
        }
    }

    function test_theImageScrollerShowsWhenItHasKeyboardFocus() {
        const {dialog} = shown({previewImage: tallImage(200, 300)})
        const indicator = required(dialog, "captureImageFocusRing")
        required(dialog, "captureImageScroll").forceActiveFocus(Qt.TabFocusReason)
        compare(indicator.visible, true)
        required(dialog, "captureCloseButton").forceActiveFocus(Qt.TabFocusReason)
        compare(indicator.visible, false)
    }

    function test_theUnavailableMessageIsAnAlert() {
        const {dialog} = shown({previewState: "invalid", canOutput: false, previewError: "Too wide to render"})
        compare(required(dialog, "captureError").Accessible.role, Accessible.AlertMessage)
    }

    function test_thePreviewStripAnnouncesItsFeedback() {
        const {dialog} = shown({feedbackKind: "failure", feedbackText: "Could not copy Run graph"})
        compare(required(dialog, "captureStatusStrip").announces, true)
    }
}
