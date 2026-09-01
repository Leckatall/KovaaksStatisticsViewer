import QtQuick
import QtTest
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "FirstRunBannerTest"

    Component {
        id: bannerComponent
        FirstRunBanner {}
    }

    function test_chooseFolderRequested_emittedWhenButtonClicked() {
        const banner = createTemporaryObject(bannerComponent, testCase)
        verify(banner !== null, "FirstRunBanner failed to instantiate")

        let emitted = 0
        banner.chooseFolderRequested.connect(() => emitted++)

        const button = ItemLookup.findByObjectName(banner, "chooseKovaaksFolderButton")
        verify(button !== null, "no choose-folder button found")
        button.click()

        compare(emitted, 1)
    }
}
