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
        verify(toolBar !== null)

        const label = findChild(toolBar, "kovaaksDirLabel")
        verify(label !== null)
        verify(label.text.indexOf("C:/Kovaaks") !== -1)
    }

    function test_labelUpdatesWhenKovaaksDirChanges() {
        const toolBar = createTemporaryObject(toolBarComponent, testCase, {
            kovaaksDir: "file:///C:/Kovaaks"
        })
        verify(toolBar !== null)
        const label = findChild(toolBar, "kovaaksDirLabel")

        toolBar.kovaaksDir = "file:///D:/OtherDir"

        verify(label.text.indexOf("D:/OtherDir") !== -1)
        verify(label.text.indexOf("C:/Kovaaks") === -1)
    }
}
