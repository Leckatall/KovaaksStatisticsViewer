import QtQuick
import QtTest
import KovaaksStatsViewer

TestCase {
    name: "ThemeTest"

    function test_classificationColor_mapsEveryClassification() {
        verify(Qt.colorEqual(Theme.classificationColor("Trackable"), Theme.success))
        verify(Qt.colorEqual(Theme.classificationColor("Incomplete"), Theme.warning))
        verify(Qt.colorEqual(Theme.classificationColor("Invalid"), Theme.error))
        verify(Qt.colorEqual(Theme.classificationColor("Unsupported"), Theme.classificationColor("")))
    }

    function test_classificationColor_unknownFallsBackToGrey() {
        verify(Qt.colorEqual(Theme.classificationColor("Nonsense"), "#9e9e9e"))
        verify(Qt.colorEqual(Theme.classificationColor(""), "#9e9e9e"))
        verify(Qt.colorEqual(Theme.classificationColor(undefined), "#9e9e9e"))
    }

    function test_readableOn_returnsAReadableColourUnchanged() {
        verify(Qt.colorEqual(Theme.readableOn("#ffc828", "#3c3c3c", "#ffffff"), "#ffc828"))
    }

    function test_readableOn_blendsTowardFallbackUntilItReads() {
        const mixed = Theme.readableOn("#82828c", "#3c3c3c", "#ffffff")
        verify(!Qt.colorEqual(mixed, "#82828c"), "a low-contrast colour is changed")
        verify(mixed.r > 0x82 / 255, "and moves toward the fallback")
        const onLight = Theme.readableOn("#ffc828", "#efefef", "#000000")
        verify(!Qt.colorEqual(onLight, "#ffc828"))
        verify(onLight.r < 1, "it darkens toward a dark fallback")
    }

    function test_readableOn_emptyColourIsTheFallback() {
        verify(Qt.colorEqual(Theme.readableOn("", "#3c3c3c", "#ffffff"), "#ffffff"))
    }

    function test_isLightFill_splitsOnPerceivedBrightness() {
        verify(Theme.isLightFill("#ffffff"))
        verify(Theme.isLightFill("#ffc828"))
        verify(!Theme.isLightFill("#000000"))
        verify(!Theme.isLightFill("#82828c"))
    }
}
