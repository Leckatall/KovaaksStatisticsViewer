import QtQuick
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase
    name: "FirstRunBannerTest"

    Component {
        id: bannerComponent
        FirstRunBanner {}
    }

    function findByObjectName(item, objectName) {
        if (item.objectName === objectName) return item
        for (const child of item.children) {
            const match = findByObjectName(child, objectName)
            if (match) return match
        }
        return null
    }

    function test_chooseFolderRequested_emittedWhenButtonClicked() {
        const banner = createTemporaryObject(bannerComponent, testCase)
        verify(banner !== null, "FirstRunBanner failed to instantiate")

        let emitted = 0
        banner.chooseFolderRequested.connect(() => emitted++)

        const button = findByObjectName(banner, "chooseKovaaksFolderButton")
        verify(button !== null, "no choose-folder button found")
        button.click()

        compare(emitted, 1)
    }
}
