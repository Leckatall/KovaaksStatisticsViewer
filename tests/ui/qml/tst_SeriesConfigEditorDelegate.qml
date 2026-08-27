import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml"

TestCase {
    id: testCase

    property var row: ({
        id: "1",
        name: "Score",
        color: "#009600",
        width: 2,
        enabled: true,
        displayPosition: 0,
        expression: { kind: "primitive", primitiveMetric: "score" }
    })

    SignalSpy { id: colorRequestedSpy; signalName: "colorRequested" }
    SignalSpy { id: nameUpdateRequestedSpy; signalName: "nameUpdateRequested" }
    SignalSpy { id: expressionEditRequestedSpy; signalName: "expressionEditRequested" }
    SignalSpy { id: axisSelectionRequestedSpy; signalName: "axisSelectionRequested" }
    SignalSpy { id: seriesRemovalRequestedSpy; signalName: "seriesRemovalRequested" }
    SignalSpy { id: seriesEnabledRequestedSpy; signalName: "seriesEnabledRequested" }

    function createDelegate() {
        const delegate = createTemporaryObject(delegateComponent, testCase, { modelData: row })
        verify(delegate !== null)
        colorRequestedSpy.target = delegate
        nameUpdateRequestedSpy.target = delegate
        expressionEditRequestedSpy.target = delegate
        axisSelectionRequestedSpy.target = delegate
        seriesRemovalRequestedSpy.target = delegate
        seriesEnabledRequestedSpy.target = delegate
        return delegate
    }

    function findByObjectName(root, name) {
        if (root.objectName === name)
            return root
        for (const child of root.children || []) {
            const found = findByObjectName(child, name)
            if (found)
                return found
        }
        return null
    }

    function findByObjectNamePrefix(root, prefix, matches) {
        if (root.objectName && root.objectName.startsWith(prefix))
            matches.push(root)
        for (const child of root.children || [])
            findByObjectNamePrefix(child, prefix, matches)
    }

    function test_dragHandleShowsSixDotsInTwoColumnsAndThreeRows() {
        const delegate = createDelegate()
        const handle = findByObjectName(delegate, "seriesDragHandle_" + row.id)
        verify(handle !== null)

        const dots = []
        findByObjectNamePrefix(handle, "seriesDragHandleDot_" + row.id + "_", dots)
        compare(dots.length, 6)

        for (let index = 0; index < dots.length; ++index) {
            compare(dots[index].width, 3)
            compare(dots[index].height, 3)
            compare(dots[index].color, delegate.dragHandleDotColor)
            compare(dots[index].x, 1.5 + (index % 2) * 6)
            compare(dots[index].y, 4.5 + Math.floor(index / 2) * 6)
        }
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

    function test_actionSignalsExposeTheirPayload() {
        const delegate = createDelegate()

        delegate.colorRequested(row)
        compare(colorRequestedSpy.count, 1)
        compare(colorRequestedSpy.signalArguments[0][0], row)

        delegate.nameUpdateRequested(row, "New score")
        compare(nameUpdateRequestedSpy.count, 1)
        compare(nameUpdateRequestedSpy.signalArguments[0][0], row)
        compare(nameUpdateRequestedSpy.signalArguments[0][1], "New score")

        delegate.expressionEditRequested(row)
        compare(expressionEditRequestedSpy.count, 1)
        compare(expressionEditRequestedSpy.signalArguments[0][0], row)

        delegate.axisSelectionRequested(row, "axis-1")
        compare(axisSelectionRequestedSpy.count, 1)
        compare(axisSelectionRequestedSpy.signalArguments[0][0], row)
        compare(axisSelectionRequestedSpy.signalArguments[0][1], "axis-1")

        delegate.seriesRemovalRequested(row)
        compare(seriesRemovalRequestedSpy.count, 1)
        compare(seriesRemovalRequestedSpy.signalArguments[0][0], row)

        delegate.seriesEnabledRequested(row, false)
        compare(seriesEnabledRequestedSpy.count, 1)
        compare(seriesEnabledRequestedSpy.signalArguments[0][0], row)
        compare(seriesEnabledRequestedSpy.signalArguments[0][1], false)
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
