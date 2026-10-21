import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/series"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase

    property var row: ({
        id: "1",
        name: "Score",
        color: "#009600",
        width: 2,
        enabled: true,
        displayPosition: 0,
        hiddenLeadingPoints: 0,
        expression: { kind: "primitive", primitiveMetric: "score" }
    })

    SignalSpy { id: optionsRequestedSpy; signalName: "optionsRequested" }

    function createDelegate() {
        const delegate = createTemporaryObject(delegateComponent, testCase, { modelData: row })
        verify(delegate !== null)
        optionsRequestedSpy.target = delegate
        optionsRequestedSpy.clear()
        return delegate
    }

    function test_delegateLightensOnHover() {
        const delegate = createDelegate()
        compare(delegate.radius, 6)
        verify(delegate.restingColor !== delegate.palette.window)
        compare(delegate.color, delegate.restingColor)

        mouseMove(delegate, 1, 1)
        tryVerify(function() { return delegate.hovered })
        compare(delegate.color, delegate.hoverColor)

        mouseMove(delegate, 1, delegate.height + 10)
        tryVerify(function() { return !delegate.hovered })
        compare(delegate.color, delegate.restingColor)
    }

    function test_clickingTheOptionsButtonRequestsOptionsAnchoredToIt() {
        const delegate = createDelegate()
        const options = ItemLookup.findByObjectName(delegate, "seriesOptionsButton_" + row.id)
        mouseClick(options)
        compare(optionsRequestedSpy.count, 1)
        compare(optionsRequestedSpy.signalArguments[0][0], row)
        compare(optionsRequestedSpy.signalArguments[0][1], options)
    }

    function test_theIndicatorAndTooltipReflectHiddenLeadingPoints() {
        const delegate = createDelegate()
        const indicator = ItemLookup.findByObjectName(delegate, "seriesOptionsIndicator_" + row.id)
        const options = ItemLookup.findByObjectName(delegate, "seriesOptionsButton_" + row.id)
        verify(indicator !== null)
        verify(!indicator.visible)

        const hiding = createTemporaryObject(delegateComponent, testCase, {
            modelData: Object.assign({}, row, { hiddenLeadingPoints: 3 })
        })
        const hidingIndicator = ItemLookup.findByObjectName(hiding, "seriesOptionsIndicator_" + row.id)
        const hidingOptions = ItemLookup.findByObjectName(hiding, "seriesOptionsButton_" + row.id)
        verify(hidingIndicator.visible)
        compare(hidingOptions.ToolTip.text, "Hiding the first 3 points")
        compare(hidingOptions.Accessible.name, "Hiding the first 3 points")
        compare(options.ToolTip.text, "Series options")

        const hidingOne = createTemporaryObject(delegateComponent, testCase, {
            modelData: Object.assign({}, row, { hiddenLeadingPoints: 1 })
        })
        compare(ItemLookup.findByObjectName(hidingOne, "seriesOptionsButton_" + row.id).ToolTip.text,
                "Hiding the first point")
    }

    function test_theIndicatorSitsOnTheButtonCornerClearOfTheGlyph() {
        const delegate = createTemporaryObject(delegateComponent, testCase, {
            modelData: Object.assign({}, row, { hiddenLeadingPoints: 2 })
        })
        verify(waitForRendering(delegate))
        const options = ItemLookup.findByObjectName(delegate, "seriesOptionsButton_" + row.id)
        const indicator = ItemLookup.findByObjectName(delegate, "seriesOptionsIndicator_" + row.id)
        const glyph = options.contentItem
        const glyphTop = glyph.mapToItem(options, 0, (glyph.height - options.icon.height) / 2).y
        const indicatorBottom = indicator.mapToItem(options, 0, indicator.height).y
        verify(indicatorBottom <= glyphTop, "indicator bottom " + indicatorBottom + " overlaps glyph top " + glyphTop)
        verify(indicator.mapToItem(options, indicator.width, 0).x > options.width, "indicator should overhang the corner")
    }

    Component {
        id: delegateComponent

        SeriesConfigEditorDelegate {
            displayRows: [testCase.row]
            dragState: QtObject {
                property string draggedSeriesId: ""
                property int dragOriginIndex: -1
                property int dragPreviewIndex: -1
                property real dragTranslationY: 0
                property real dragRawTranslationY: 0
                property real dragStartPointerY: 0
                property real dragRowHeight: 0
                function previewMove(position) {}
            }
            scrollView: QtObject {
                property real autoScrollAccumulatedDelta: 0
                property int autoScrollDirection: 0
                property real autoScrollEdgeThreshold: 32
                property real height: 100
            }
            axisComboModel: [
                { text: "Independent", value: "" },
                { text: "Axis", value: "axis-1" }
            ]
        }
    }

    height: 400
    name: "SeriesConfigEditorDelegateTest"
    visible: true
    when: windowShown
    width: 800
}
