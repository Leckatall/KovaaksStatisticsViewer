import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "KovaaksDirToolBarTest"
    when: windowShown
    width: 400
    height: 100
    visible: true

    Component {
        id: toolBarComponent
        KovaaksDirToolBar {}
    }

    function test_labelDisplaysKovaaksDir() {
        const toolBar = createTemporaryObject(toolBarComponent, testCase, {
            kovaaksDir: "file:///C:/Kovaaks"
        })
        verify(toolBar !== null, "KovaaksDirToolBar failed to instantiate")

        const label = findChild(toolBar, "kovaaksDirLabel")
        verify(label !== null, "no child named 'kovaaksDirLabel' found in KovaaksDirToolBar")
        verify(label.text.indexOf("C:/Kovaaks") !== -1,
            "expected label text to contain 'C:/Kovaaks', got: " + label.text)
    }

    function test_labelUpdatesWhenKovaaksDirChanges() {
        const toolBar = createTemporaryObject(toolBarComponent, testCase, {
            kovaaksDir: "file:///C:/Kovaaks"
        })
        verify(toolBar !== null, "KovaaksDirToolBar failed to instantiate")
        const label = findChild(toolBar, "kovaaksDirLabel")

        toolBar.kovaaksDir = "file:///D:/OtherDir"

        verify(label.text.indexOf("D:/OtherDir") !== -1,
            "expected label text to contain updated dir 'D:/OtherDir', got: " + label.text)
        verify(label.text.indexOf("C:/Kovaaks") === -1,
            "expected label text to no longer contain stale dir 'C:/Kovaaks', got: " + label.text)
    }
}
