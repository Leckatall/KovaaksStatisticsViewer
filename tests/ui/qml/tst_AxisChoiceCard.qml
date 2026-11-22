import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtTest
import "../../../src/ui/qml/graph"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "AxisChoiceCardTest"
    when: windowShown
    width: 600
    height: 400
    visible: true

    Component {
        id: plainFixture

        Column {
            property alias card: card
            property alias before: before
            width: 400

            TextField {
                id: before
                objectName: "before"
                width: parent.width
            }

            AxisChoiceCard {
                id: card
                width: parent.width
                choiceKey: "runs"
                choiceName: "Run"
                detailText: detail.text

                Label {
                    id: detail
                    objectName: "detail"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    wrapMode: Text.WordWrap
                    text: "Each run takes equal width, labelled with dates, however narrow the editor becomes."
                }
            }
        }
    }

    Component {
        id: headerFixture

        AxisChoiceCard {
            id: card
            property alias field: field
            property alias trailingButton: trailing
            width: 400
            choiceKey: "axis:1"
            choiceName: "Score"
            header: [
                TextField {
                    id: field
                    objectName: "field"
                    Layout.fillWidth: true
                    text: "Score"
                },
                Button {
                    id: trailing
                    objectName: "action"
                    text: "X"
                }
            ]

            Label {
                objectName: "detail"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                wrapMode: Text.WordWrap
                text: "Used by 6 lines"
            }
        }
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }

    function find(root, name) {
        const found = ItemLookup.findByObjectName(root, name)
        verify(found !== null, "no item " + name)
        return found
    }

    function makePlain(width) {
        const fixture = createTemporaryObject(plainFixture, testCase, width ? {width: width} : {})
        verify(waitForRendering(fixture))
        return fixture
    }

    function makeWithHeader() {
        const card = createTemporaryObject(headerFixture, testCase)
        verify(waitForRendering(card))
        return card
    }

    function chosenSpy(card) {
        return createTemporaryObject(spyComponent, testCase, {target: card, signalName: "chosen"})
    }

    function test_everyPartOfTheCardChoosesIt_data() {
        return [
            {tag: "marker", target: "axisChoiceMarker"},
            {tag: "name", target: "axisChoiceName"},
            {tag: "detail", target: "detail"},
            {tag: "padding", target: ""}
        ]
    }

    function test_everyPartOfTheCardChoosesIt(data) {
        const card = makePlain().card
        const spy = chosenSpy(card)

        if (data.target === "")
            mouseClick(card, card.width - 3, card.height - 3)
        else
            mouseClick(find(card, data.target))

        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "runs")
    }

    function test_choosingTheSelectedCardDoesNothing() {
        const card = makePlain().card
        card.selected = true
        const spy = chosenSpy(card)

        mouseClick(card)

        compare(spy.count, 0)
        compare(card.checked, true)
    }

    // `checked` mirrors the view model's flag, so a revert or Cancel restores it.
    function test_checkedFollowsSelectedRatherThanTheClick() {
        const card = makePlain().card

        mouseClick(card)
        compare(card.checked, false)

        card.selected = true
        compare(card.checked, true)
        card.selected = false
        compare(card.checked, false)
    }

    function test_headerControlsKeepTheirOwnClicks() {
        const card = makeWithHeader()
        const spy = chosenSpy(card)
        const actionSpy = createTemporaryObject(spyComponent, testCase, {target: card.trailingButton, signalName: "clicked"})

        mouseClick(card.field)
        mouseClick(card.trailingButton)

        compare(spy.count, 0)
        compare(actionSpy.count, 1)
        verify(!find(card, "axisChoiceName").visible, "the header replaces the plain name")
    }

    // A TextInput leaves Return unaccepted after emitting editingFinished, so it reaches the card.
    function test_returnInAHeaderFieldDoesNotChooseTheCard() {
        const card = makeWithHeader()
        const spy = chosenSpy(card)
        card.field.forceActiveFocus()

        keyClick(Qt.Key_Return)
        keyClick(Qt.Key_Enter)
        keyClick(Qt.Key_Space)

        compare(spy.count, 0)
    }

    function test_spaceAndEnterChooseTheFocusedCard_data() {
        return [{tag: "space", key: Qt.Key_Space}, {tag: "return", key: Qt.Key_Return}, {tag: "enter", key: Qt.Key_Enter}]
    }

    function test_spaceAndEnterChooseTheFocusedCard(data) {
        const card = makePlain().card
        const spy = chosenSpy(card)
        card.forceActiveFocus(Qt.TabFocusReason)

        keyClick(data.key)

        compare(spy.count, 1)
    }

    function test_tabReachesTheCardBeforeItsHeaderControls() {
        const card = makeWithHeader()
        card.forceActiveFocus(Qt.TabFocusReason)

        keyClick(Qt.Key_Tab)
        compare(testCase.Window.window.activeFocusItem, card.field)
        keyClick(Qt.Key_Tab)
        compare(testCase.Window.window.activeFocusItem, card.trailingButton)
    }

    function test_theFocusIndicatorShowsOnlyForKeyboardFocus() {
        const fixture = makePlain()
        const card = fixture.card
        compare(card.focusIndicator.visible, false)

        fixture.before.forceActiveFocus()
        keyClick(Qt.Key_Tab)
        verify(card.activeFocus)
        compare(card.focusIndicator.visible, true)

        fixture.before.forceActiveFocus()
        mouseClick(card)
        verify(card.activeFocus)
        compare(card.focusIndicator.visible, false)
    }

    function test_theFocusIndicatorIsIndependentOfSelection() {
        const fixture = makePlain()
        const card = fixture.card
        card.selected = true
        compare(card.focusIndicator.visible, false)

        card.selected = false
        fixture.before.forceActiveFocus()
        keyClick(Qt.Key_Tab)
        compare(card.focusIndicator.visible, true)
    }

    function test_assistiveTechnologySeesARadioButton() {
        const card = makePlain().card
        card.selected = true

        compare(card.Accessible.role, Accessible.RadioButton)
        compare(card.Accessible.checkable, true)
        compare(card.Accessible.checked, true)
        compare(card.Accessible.name, "Run")
        compare(card.Accessible.description, find(card, "detail").text)

        card.selected = false
        compare(card.Accessible.checked, false)
    }

    function test_detailsWrapInsideANarrowCard() {
        const card = makePlain(220).card
        const detail = find(card, "detail")

        verify(detail.lineCount > 1, "the detail wraps")
        verify(detail.mapToItem(card, detail.width, 0).x <= card.width - card.rightPadding + 0.5,
               "the detail stays inside the card")
        const name = find(card, "axisChoiceName")
        const marker = find(card, "axisChoiceMarker")
        fuzzyCompare(detail.mapToItem(card, 0, 0).x, name.mapToItem(card, 0, 0).x, 0.5,
                     "the detail lines up with the name")
        verify(name.mapToItem(card, 0, 0).x >= marker.mapToItem(card, marker.width, 0).x,
               "the name sits beside the marker")
    }
}
