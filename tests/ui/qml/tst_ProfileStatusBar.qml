import QtQuick
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "ProfileStatusBarTest"
    width: 700
    height: 400
    when: windowShown
    visible: true

    Component {
        id: vmComponent
        QtObject {
            property bool profileImportBusy: false
            property string profileImportPhase: "idle"
            property string profileImportMode: "refresh"
            property string profileImportOutcome: "none"
            property int profileImportDone: 0
            property int profileImportTotal: 0
            property double profileImportProgress: 0
            property int profileImportAdded: 0
            property int profileImportEnriched: 0
            property var profileImportIssues: []
            property bool profileImportSaveFailed: false
        }
    }
    Component { id: barComponent; ProfileStatusBar { width: 650; noticeDuration: 100 } }
    Component { id: captureStubComponent; CaptureVmStub {} }
    SignalSpy { id: detailsSpy; signalName: "detailsRequested" }

    function makeBar(vmProps) {
        const vm = createTemporaryObject(vmComponent, testCase, vmProps || {})
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm})
        return {vm, bar}
    }
    function find(bar, name) { return ItemLookup.findByObjectName(bar, name) }

    function finishImport(vm, props) {
        Object.assign(vm, props)
        vm.profileImportBusy = false
    }
    function runImport(vm, props) {
        vm.profileImportBusy = true
        finishImport(vm, props)
    }

    function test_acceptsACaptureViewModel() {
        const vm = createTemporaryObject(vmComponent, testCase)
        const capture = createTemporaryObject(captureStubComponent, testCase)
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm, captureVm: capture})
        verify(bar.captureVm === capture, "captureVm was not accepted")
    }

    function test_captureFeedbackCoexistsWithAnUnresolvedWarning() {
        const vm = createTemporaryObject(vmComponent, testCase, {
            profileImportOutcome: "partial",
            profileImportIssues: [{path: "a.perf", message: "x", retryable: true}]
        })
        const capture = createTemporaryObject(captureStubComponent, testCase,
            {feedbackKind: "success", feedbackText: "Copied a graph with a deliberately long descriptive name"})
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm, captureVm: capture, width: 400})
        compare(bar.mode, "warning")
        const warning = find(bar, "profileStatusText")
        const capText = find(bar, "captureStatusText")
        verify(capText !== null, "the bar has no capture status text")
        compare(warning.visible, true)
        compare(find(bar, "profileStatusDetailsButton").visible, true)
        compare(find(bar, "profileStatusDismissButton").visible, true)
        compare(capText.visible, true)
        const warningBox = warning.mapToItem(bar, 0, 0)
        const capBox = capText.mapToItem(bar, 0, 0)
        verify(capBox.x >= warningBox.x + warning.width, "capture status must sit beside the warning, not overlap it")
        verify(Math.abs((capBox.y + capText.height / 2) - (warningBox.y + warning.height / 2)) < 2,
               "capture status must share the warning's row, not stack")
        verify(warning.truncated, "the warning should elide rather than push the capture status out")
        verify(capText.truncated, "the capture status should elide rather than overflow")
        verify(capBox.x + capText.width <= bar.width, "capture status lies outside the bar")
    }

    function test_theBarStaysSilentWhileThePreviewAnnouncesInstead() {
        const vm = createTemporaryObject(vmComponent, testCase)
        const capture = createTemporaryObject(captureStubComponent, testCase,
            {feedbackKind: "success", feedbackText: "Copied a graph"})
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm, captureVm: capture})
        const strip = find(bar, "captureStatusStrip")
        compare(strip.announces, true)
        capture.previewOpen = true
        compare(strip.announces, false)
        capture.previewOpen = false
        compare(strip.announces, true)
    }

    function test_captureFeedbackAppearsDuringImport() {
        const vm = createTemporaryObject(vmComponent, testCase, {profileImportBusy: true, profileImportPhase: "scanning"})
        const capture = createTemporaryObject(captureStubComponent, testCase,
            {feedbackKind: "failure", feedbackText: "Could not copy a graph"})
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm, captureVm: capture})
        compare(bar.mode, "busy")
        compare(find(bar, "profileStatusProgressBar").visible, true)
        const capText = find(bar, "captureStatusText")
        verify(capText !== null, "the bar has no capture status text")
        compare(capText.visible, true)
    }

    function test_idleHidesTheFooter() {
        const {vm, bar} = makeBar()
        compare(bar.mode, "idle")
        compare(bar.visible, false)
        vm.profileImportOutcome = "stale"
        compare(bar.visible, false)
        runImport(vm, {profileImportOutcome: "succeeded"})
        compare(bar.mode, "idle")
        compare(bar.visible, false)
    }

    function test_captureFeedbackShowsTheFooterUntilItClears() {
        const vm = createTemporaryObject(vmComponent, testCase)
        const capture = createTemporaryObject(captureStubComponent, testCase)
        const bar = createTemporaryObject(barComponent, testCase, {sessionVm: vm, captureVm: capture})
        compare(bar.visible, false)
        capture.feedbackKind = "success"
        capture.feedbackText = "Copied a graph"
        compare(bar.visible, true)
        capture.feedbackKind = "none"
        compare(bar.visible, false)
    }

    function test_importStatusShowsTheFooterAndExpiryHidesIt() {
        const {vm, bar} = makeBar()
        compare(bar.visible, false)
        vm.profileImportBusy = true
        compare(bar.visible, true)
        finishImport(vm, {profileImportOutcome: "succeeded", profileImportAdded: 1})
        compare(bar.visible, true)
        tryCompare(bar, "visible", false)
    }

    function test_dismissingTheLastWarningHidesTheFooter() {
        const {bar} = makeBar({profileImportOutcome: "partial", profileImportIssues: [{path: "a", message: "x"}]})
        compare(bar.visible, true)
        mouseClick(find(bar, "profileStatusDismissButton"))
        compare(bar.visible, false)
    }

    function test_busyShowsProgressText() {
        const {vm, bar} = makeBar({profileImportBusy: true, profileImportPhase: "scanning"})
        const progress = find(bar, "profileStatusProgressBar")
        compare(bar.mode, "busy")
        compare(find(bar, "profileStatusText").text, "Scanning sources…")
        compare(progress.indeterminate, true)
        vm.profileImportPhase = "importing"
        vm.profileImportTotal = 4
        vm.profileImportDone = 2
        vm.profileImportProgress = 0.5
        compare(progress.indeterminate, false)
        compare(progress.value, 0.5)
        compare(find(bar, "profileStatusText").text, "Refreshing profile: 2 of 4 runs")
        vm.profileImportMode = "rebuild"
        compare(find(bar, "profileStatusText").text, "Rebuilding profile: 2 of 4 runs")
        vm.profileImportPhase = "saving"
        compare(find(bar, "profileStatusText").text, "Saving profile…")
        compare(progress.indeterminate, true)
    }

    function test_addedRunsShowATransientNotice() {
        const {vm, bar} = makeBar()
        runImport(vm, {profileImportOutcome: "succeeded", profileImportAdded: 2})
        compare(bar.mode, "notice")
        compare(find(bar, "profileStatusText").text, "Added 2 runs")
        tryCompare(bar, "mode", "idle")
    }

    function test_noticeCopyForEnrichedAndMixedCounts() {
        let made = makeBar()
        runImport(made.vm, {profileImportOutcome: "succeeded", profileImportEnriched: 1})
        compare(find(made.bar, "profileStatusText").text, "Updated 1 run")
        made = makeBar()
        runImport(made.vm, {profileImportOutcome: "succeeded", profileImportAdded: 1, profileImportEnriched: 3})
        compare(find(made.bar, "profileStatusText").text, "Added 1 run · updated 3")
    }

    function test_newIssuesRaiseAWarningThatStays() {
        const {vm, bar} = makeBar()
        runImport(vm, {
            profileImportOutcome: "partial", profileImportAdded: 1,
            profileImportIssues: [{path: "a.perf", message: "x", retryable: true}]
        })
        compare(bar.mode, "warning")
        wait(300)
        compare(bar.mode, "warning")
        compare(find(bar, "profileStatusText").text, "1 source file could not be imported")
        vm.profileImportIssues = [{path: "a", message: "x"}, {path: "b", message: "y"}]
        compare(find(bar, "profileStatusText").text, "2 source files could not be imported")
    }

    function test_saveFailureAndFailedHaveTheirOwnCopy() {
        const {vm, bar} = makeBar({profileImportOutcome: "failed"})
        compare(bar.mode, "warning")
        compare(find(bar, "profileStatusText").text, "Profile refresh failed.")
        vm.profileImportSaveFailed = true
        compare(find(bar, "profileStatusText").text, "Could not save profile. Your previous profile is unchanged.")
    }

    function test_previouslyFailedIssuesDoNotWarn() {
        const issues = [{path: "a.perf", message: "x", retryable: false, previouslyFailed: true}]
        let made = makeBar()
        runImport(made.vm, {profileImportOutcome: "partial", profileImportIssues: issues})
        compare(made.bar.mode, "idle")
        made = makeBar()
        runImport(made.vm, {profileImportOutcome: "partial", profileImportAdded: 1, profileImportIssues: issues})
        compare(made.bar.mode, "notice")
    }

    function test_dismissHidesUntilTheNextCompletedImport() {
        const {vm, bar} = makeBar()
        const issues = [{path: "a.perf", message: "x", retryable: true}]
        runImport(vm, {profileImportOutcome: "partial", profileImportIssues: issues})
        mouseClick(find(bar, "profileStatusDismissButton"))
        compare(bar.mode, "idle")
        runImport(vm, {profileImportOutcome: "partial", profileImportIssues: issues})
        compare(bar.mode, "warning")
    }

    function test_detailsButtonRequestsDetails() {
        const {vm, bar} = makeBar({profileImportOutcome: "partial", profileImportIssues: [{path: "a", message: "x"}]})
        detailsSpy.target = bar
        detailsSpy.clear()
        mouseClick(find(bar, "profileStatusDetailsButton"))
        compare(detailsSpy.count, 1)
    }
}
