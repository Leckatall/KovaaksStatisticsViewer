import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/series"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "AxisNameDialogTest"
    when: windowShown
    width: 600
    height: 400
    visible: true

    Component {
        id: dialogComponent
        AxisNameDialog {}
    }

    function makeFakeSettingsVm(overrides) {
        return TestDoubles.makeFakeSettingsVm(Object.assign({
            updateSeriesAxisCalls: [],
            updateSeriesAxis: function (lineId, axisId) {
                this.updateSeriesAxisCalls.push([lineId, axisId])
                return {succeeded: true}
            }
        }, overrides))
    }

    function openDialog(vm, lineId) {
        const dialog = createTemporaryObject(dialogComponent, testCase, {settingsVm: vm})
        verify(dialog !== null)
        dialog.openFor(lineId)
        tryCompare(dialog, "opened", true)
        return dialog
    }

    function button(dialog, name) {
        const found = ItemLookup.findByObjectName(dialog.footer, name)
        verify(found !== null, "no button " + name)
        return found
    }

    function test_invalidCreateKeepsTheDialogOpenWithAnErrorAndTheTypedText() {
        const vm = makeFakeSettingsVm({createAxisResult: {succeeded: false, issue: "axisNameDuplicate"}})
        const dialog = openDialog(vm, "1")
        dialog.nameField.text = "Score"

        button(dialog, "newAxisCreateButton").clicked()

        compare(dialog.opened, true)
        compare(dialog.nameField.text, "Score")
        compare(dialog.errorLabel.visible, true)
        compare(dialog.settingsVm.updateSeriesAxisCalls.length, 0)
    }

    function test_createFromALineAssignsTheNewAxisToThatLine() {
        const vm = makeFakeSettingsVm()
        const dialog = openDialog(vm, "4")
        dialog.nameField.text = "Custom"
        const createdSpy = createTemporaryObject(spyComponent, testCase, {target: dialog, signalName: "created"})

        button(dialog, "newAxisCreateButton").clicked()

        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
        compare(dialog.settingsVm.updateSeriesAxisCalls, [["4", "9"]])
        compare(createdSpy.count, 1)
        compare(createdSpy.signalArguments[0][0], "9")
        tryCompare(dialog, "opened", false)
    }

    function test_createWithoutALineLeavesTheAxisUnassigned() {
        const vm = makeFakeSettingsVm()
        const dialog = openDialog(vm, "")
        dialog.nameField.text = "Custom"

        button(dialog, "newAxisCreateButton").clicked()

        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
        compare(dialog.settingsVm.updateSeriesAxisCalls.length, 0)
        tryCompare(dialog, "opened", false)
    }

    function test_cancelCallsNothing() {
        const vm = makeFakeSettingsVm()
        const dialog = openDialog(vm, "1")
        dialog.nameField.text = "Custom"

        button(dialog, "newAxisCancelButton").clicked()

        compare(dialog.settingsVm.createAxisCalls.length, 0)
        compare(dialog.settingsVm.updateSeriesAxisCalls.length, 0)
        tryCompare(dialog, "opened", false)
    }

    function test_theAxisNameLabelStaysVisibleWhileTyping() {
        const dialog = openDialog(makeFakeSettingsVm(), "1")
        const label = ItemLookup.findByObjectName(dialog.contentItem, "newAxisNameLabel")
        verify(label !== null)
        compare(label.visible, true)

        dialog.nameField.text = "Cust"

        compare(label.visible, true)
    }

    function test_enterInTheFieldCreates() {
        const dialog = openDialog(makeFakeSettingsVm(), "")
        tryVerify(() => dialog.nameField.activeFocus)

        for (const character of "Custom")
            keyClick(character)
        keyClick(Qt.Key_Return)

        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
        tryCompare(dialog, "opened", false)
    }

    function test_enterOnAFocusedFooterButtonActivatesIt() {
        const dialog = openDialog(makeFakeSettingsVm(), "")
        dialog.nameField.text = "Custom"
        button(dialog, "newAxisCreateButton").forceActiveFocus()

        keyClick(Qt.Key_Return)

        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
        tryCompare(dialog, "opened", false)

        dialog.openFor("")
        tryCompare(dialog, "opened", true)
        button(dialog, "newAxisCancelButton").forceActiveFocus()

        keyClick(Qt.Key_Enter)

        tryCompare(dialog, "opened", false)
        compare(dialog.settingsVm.createAxisCalls, ["Custom"])
    }

    function test_reopeningClearsThePreviousTextAndError() {
        const vm = makeFakeSettingsVm({createAxisResult: {succeeded: false, issue: "axisNameBlank"}})
        const dialog = openDialog(vm, "1")
        dialog.nameField.text = "  "
        button(dialog, "newAxisCreateButton").clicked()
        compare(dialog.errorLabel.visible, true)
        button(dialog, "newAxisCancelButton").clicked()
        tryCompare(dialog, "opened", false)

        dialog.openFor("")
        tryCompare(dialog, "opened", true)

        compare(dialog.nameField.text, "")
        compare(dialog.errorLabel.text, "")
        compare(dialog.errorLabel.visible, false)
        compare(dialog.assignLineId, "")
        tryVerify(() => dialog.nameField.activeFocus)
    }

    function test_theFieldCarriesItsErrorForAssistiveTechnology() {
        const vm = makeFakeSettingsVm({createAxisResult: {succeeded: false, issue: "axisNameDuplicate"}})
        const dialog = openDialog(vm, "1")
        compare(dialog.nameField.Accessible.description, "")
        dialog.nameField.text = "Score"

        button(dialog, "newAxisCreateButton").clicked()

        verify(dialog.nameField.Accessible.description.length > 0)
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }
}
