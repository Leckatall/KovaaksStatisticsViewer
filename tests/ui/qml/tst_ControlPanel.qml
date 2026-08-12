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
            generateProfile: function () { this.generateProfileCalls++ },
            profileBuildInProgress: false,
            profileBuildProgress: 0
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

    function test_loadLatestPerformanceButton_fetchesLatestData() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const button = findChild(panel, "loadLatestPerformanceButton")
        verify(button !== null, "no child named 'loadLatestPerformanceButton' found in ControlPanel")
        mouseClick(button)

        compare(panel.graphVm.fetchLatestDataCalls, 1, "clicking loadLatestPerformanceButton should call graphVm.fetchLatestData() once")
        compare(panel.graphVm.fetchDataCalls.length, 0, "clicking loadLatestPerformanceButton should not call graphVm.fetchData()")
    }

    function test_profileBuildProgressBar_hiddenWhenNoBuildIsRunning() {
        const panel = createPanel({
            sessionVm: makeFakeSessionVm(), graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const bar = findChild(panel, "profileBuildProgressBar")
        verify(bar !== null, "no child named 'profileBuildProgressBar' found in ControlPanel")
        compare(bar.visible, false, "the progress bar should be hidden while no build is running")
    }

    function test_profileBuildProgressBar_showsProgressWhileBuilding() {
        const vm = makeFakeSessionVm()
        vm.profileBuildInProgress = true
        vm.profileBuildProgress = 0.4
        const panel = createPanel({
            sessionVm: vm, graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const bar = findChild(panel, "profileBuildProgressBar")
        verify(bar !== null, "no child named 'profileBuildProgressBar' found in ControlPanel")
        compare(bar.visible, true, "the progress bar should be visible while a build is running")
        compare(bar.value, 0.4, "the progress bar should bind to sessionVm.profileBuildProgress")
        compare(bar.indeterminate, false, "a known fraction should not render as indeterminate")
    }

    // The file count is only known once the first per-file report lands.
    function test_profileBuildProgressBar_indeterminateBeforeTheFirstReport() {
        const vm = makeFakeSessionVm()
        vm.profileBuildInProgress = true
        const panel = createPanel({
            sessionVm: vm, graphVm: makeFakeGraphVm(), columnVisibility: ({})
        })

        const bar = findChild(panel, "profileBuildProgressBar")
        compare(bar.indeterminate, true, "a build with no progress yet should render as indeterminate")
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
}
