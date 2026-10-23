import QtQuick
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "InvalidKovaaksDirBannerTest"

    Component {
        id: bannerComponent
        InvalidKovaaksDirBanner {}
    }

    function test_chooseFolderRequested_emittedWhenButtonClicked() {
        const banner = createTemporaryObject(bannerComponent, testCase)
        verify(banner !== null, "InvalidKovaaksDirBanner failed to instantiate")

        let emitted = 0
        banner.chooseFolderRequested.connect(() => emitted++)

        const button = ItemLookup.findByObjectName(banner, "chooseKovaaksFolderButton")
        verify(button !== null, "no choose-folder button found")
        button.click()

        compare(emitted, 1)
    }
}
