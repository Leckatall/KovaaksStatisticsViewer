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

    // graphVm/sessionVm/columnVisibility are declared `property var`, so
    // ControlPanel accesses them dynamically at runtime and a plain JS
    // object stands in fine for the real (C++) view models.
    function makeFakeSessionVm(scenarioList) {
        return {
            scenario_list: scenarioList || [],
            generateProfileCalls: 0,
            getCurrentPerfScenario: function () { return "Long Jump" },
            generateProfile: function () { this.generateProfileCalls++ }
        }
    }

    function makeFakeGraphVm() {
        return TestDoubles.makeFakeGraphVm()
    }

    // Frame/ColumnLayout have no implicit size of their own outside the
    // GridLayout cell they normally live in, so without an explicit size
    // here every child ends up 0x0 and mouseClick can't hit-test anything.
    //
    // Note: initial properties passed here are cloned into the QML engine,
    // so `panel.sessionVm !== <the object literal passed in>`. Always read
    // call-tracking state back via `panel.sessionVm`/`panel.graphVm` (what's
    // returned here), never via the original object literal.
    function createPanel(props): ControlPanel {
        const panel = createTemporaryObject(controlPanelComponent, testCase,
            Object.assign({width: 400, height: 600}, props))
        verify(waitForRendering(panel), "ControlPanel never became visible/rendered")
        return panel
    }

    function test_generateProfileButton_delegatesToSessionVm() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const button = findChild(panel, "generateProfileButton")
        verify(button !== null, "no child named 'generateProfileButton' found in ControlPanel")
        mouseClick(button)

        compare(panel.sessionVm.generateProfileCalls, 1, "clicking generateProfileButton should call sessionVm.generateProfile() once")
    }

    function test_loadLatestPerformanceButton_fetchesWithEmptyId() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const button = findChild(panel, "loadLatestPerformanceButton")
        verify(button !== null, "no child named 'loadLatestPerformanceButton' found in ControlPanel")
        mouseClick(button)

        compare(panel.graphVm.fetchDataCalls.length, 1, "clicking loadLatestPerformanceButton should call graphVm.fetchData() once")
        compare(panel.graphVm.fetchDataCalls[0], "", "loadLatestPerformanceButton should fetch with an empty run id")
    }

    function test_columnVisibilityCheckBox_reflectsColumnVisibility() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(),
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
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(),
            columnVisibility: ({score: true, accuracy: false})
        })

        const scoreCheckBox = findChild(panel, "columnVisibilityCheckBox_Score")
        verify(scoreCheckBox !== null, "no child named 'columnVisibilityCheckBox_Score' found in ControlPanel")
        mouseClick(scoreCheckBox)

        compare(scoreCheckBox.checked, false, "clicking the Score checkbox should uncheck it")
        compare(panel.columnVisibility.score, false, "clicking the Score checkbox should update columnVisibility.score")
    }

    function test_renderingLabel_showsCurrentPerfScenarioFromSessionVm() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const label = findChild(panel, "renderingLabel")
        verify(label !== null, "no child named 'renderingLabel' found in ControlPanel")
        compare(label.text, "Rendering: Long Jump", "renderingLabel should show sessionVm.getCurrentPerfScenario()")
    }
}
