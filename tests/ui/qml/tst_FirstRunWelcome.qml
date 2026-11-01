import QtQuick
import QtTest
import "../../../src/ui/qml/shell"

// the first-run welcome makes choosing the Kovaaks folder the primary action.
TestCase {
    id: testCase
    name: "FirstRunWelcomeTest"
    when: windowShown
    width: 1000
    height: 640
    visible: true

    Component {
        id: welcomeComponent
        FirstRunWelcome {}
    }

    function createWelcome() {
        const welcome = createTemporaryObject(welcomeComponent, testCase, {width: 1000, height: 560})
        verify(waitForRendering(welcome))
        return welcome
    }

    function test_clickingTheButtonRequestsTheFolderDialog() {
        const welcome = createWelcome()
        let requests = 0
        welcome.chooseFolderRequested.connect(() => requests++)
        const button = findChild(welcome, "welcomeChooseKovaaksFolderButton")
        mouseClick(button, button.width / 2, button.height / 2)
        compare(requests, 1)
    }

    // Text wrapping itself is checked with the UI driver: the offscreen test
    // platform has no fonts, so glyph metrics here are placeholders.
    function test_cardStaysInsideNarrowWorkspaces() {
        const welcome = createTemporaryObject(welcomeComponent, testCase, {width: 360, height: 560})
        verify(waitForRendering(welcome))
        const card = findChild(welcome, "firstRunWelcomeTitle").parent.parent
        const left = card.mapToItem(welcome, 0, 0).x
        verify(left >= 0 && left + card.width <= welcome.width, "card spans " + left + ".." + (left + card.width))
    }
}
