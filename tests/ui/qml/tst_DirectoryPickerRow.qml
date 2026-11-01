import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "DirectoryPickerRowTest"
    when: windowShown
    width: 400
    height: 200
    visible: true

    Component {
        id: rowComponent
        DirectoryPickerRow {
            dir: "file:///C:/Kovaaks"
            label: "Kovaaks folder"
            objectNamePrefix: "kovaaksDir"
        }
    }

    SignalSpy { id: browseSpy; signalName: "browseRequested" }

    function test_clickingBrowseRequestsBrowsing() {
        const row = createTemporaryObject(rowComponent, testCase)
        verify(waitForRendering(row))
        browseSpy.target = row
        browseSpy.clear()
        mouseClick(ItemLookup.findByObjectName(row, "kovaaksDirBrowseButton"))
        compare(browseSpy.count, 1)
    }
}
