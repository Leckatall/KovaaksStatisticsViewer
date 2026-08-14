import QtQuick
import QtTest
import "../../../src/ui/qml"
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "ControlPanelTest"
    when: windowShown
    width: 500
    height: 600
    visible: true

    Component {
        id: controlPanelComponent
        ControlPanel {}
    }

    SignalSpy {
        id: configureLinesSpy
        signalName: "configureLinesRequested"
    }

    // graphVm/columnVisibility are declared `property var`, so ControlPanel
    // accesses them dynamically at runtime and a plain JS object stands in
    // fine for the real (C++) view models.
    function makeFakeGraphVm() {
        return TestDoubles.makeFakeGraphVm()
    }

    // Frame/ColumnLayout have no implicit size of their own outside the
    // GridLayout cell they normally live in, so without an explicit size
    // here every child ends up 0x0 and mouseClick can't hit-test anything.
    //
    // Note: initial properties passed here are cloned into the QML engine,
    // so `panel.graphVm !== <the object literal passed in>`. Always read
    // call-tracking state back via `panel.graphVm` (what's returned here),
    // never via the original object literal.
    function createPanel(props): ControlPanel {
        const panel = createTemporaryObject(controlPanelComponent, testCase,
            Object.assign({width: 400, height: 600}, props))
        verify(waitForRendering(panel), "ControlPanel never became visible/rendered")
        return panel
    }

    function test_columnVisibilityCheckBox_reflectsColumnVisibility() {
        const panel = createPanel({
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: false})
        })

        const scoreCheckBox = findChild(panel, "columnVisibilityCheckBox_Score")
        const accuracyCheckBox = findChild(panel, "columnVisibilityCheckBox_Accuracy")
        verify(scoreCheckBox !== null, "no child named 'columnVisibilityCheckBox_Score' found in ControlPanel")
        verify(accuracyCheckBox !== null, "no child named 'columnVisibilityCheckBox_Accuracy' found in ControlPanel")
        compare(scoreCheckBox.checked, true, "Score checkbox should reflect columnVisibility.score === true")
        compare(accuracyCheckBox.checked, false, "Accuracy checkbox should reflect columnVisibility.accuracy === false")
    }

    function test_togglingLineVisibilityCheckBox_updatesColumnVisibility() {
        const panel = createPanel({
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: false})
        })

        const scoreCheckBox = findChild(panel, "columnVisibilityCheckBox_Score")
        verify(scoreCheckBox !== null, "no child named 'columnVisibilityCheckBox_Score' found in ControlPanel")
        mouseClick(scoreCheckBox)

        compare(scoreCheckBox.checked, false, "clicking the Score checkbox should uncheck it")
        compare(panel.columnVisibility.score, false, "clicking the Score checkbox should update columnVisibility.score")
    }

    function test_onlyEnabledColumnsHaveVisibilityControls() {
        const graphVm = makeFakeGraphVm()
        graphVm.enabledColumns = [1]
        const panel = createPanel({
            graphVm: graphVm,
            columnVisibility: ({score: true, accuracy: true})
        })

        verify(findChild(panel, "columnVisibilityCheckBox_Score") !== null)
        verify(findChild(panel, "columnVisibilityCheckBox_Accuracy") === null)
    }

    function test_allDisabledStateRetainsConfigureAffordance() {
        const graphVm = makeFakeGraphVm()
        graphVm.enabledColumns = []
        const panel = createPanel({
            graphVm: graphVm,
            columnVisibility: ({score: true, accuracy: true})
        })

        const emptyLabel = findChild(panel, "noEnabledGraphLinesLabel")
        const configureButton = findChild(panel, "configureGraphLinesButton")
        verify(emptyLabel !== null)
        verify(configureButton !== null)
        compare(emptyLabel.visible, true)
        compare(configureButton.visible, true)
    }

    function test_configureButtonEmitsRequest() {
        const panel = createPanel({
            graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: true})
        })
        const configureButton = findChild(panel, "configureGraphLinesButton")
        configureLinesSpy.target = panel
        configureLinesSpy.clear()

        mouseClick(configureButton)

        tryCompare(configureLinesSpy, "count", 1)
    }
}
