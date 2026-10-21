import QtQuick
import QtTest
import "../../../src/ui/qml/shell"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "ProfileImportStatusTest"
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
    Component { id: statusComponent; ProfileImportStatus { width: 650 } }
    function find(status, name) { return ItemLookup.findByObjectName(status, name) }
    function test_progressAndTerminalResultFollowNotifyingState() {
        const vm = createTemporaryObject(vmComponent, testCase, {profileImportBusy: true, profileImportPhase: "scanning"})
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        const bar = find(status, "profileBuildProgressBar")
        compare(bar.indeterminate, true)
        vm.profileImportPhase = "importing"
        vm.profileImportTotal = 4
        vm.profileImportDone = 2
        vm.profileImportProgress = 0.5
        compare(bar.indeterminate, false)
        compare(bar.value, 0.5)
        compare(find(status, "profileBuildProgressLabel").text, "Refreshing profile: 2 of 4 runs")
        vm.profileImportBusy = false
        vm.profileImportOutcome = "succeeded"
        vm.profileImportAdded = 2
        compare(bar.visible, false)
        compare(find(status, "profileImportSummary").text, "Added 2 runs; updated 0 runs.")
    }
    function test_noopAndSaveFailureAreDistinct() {
        const vm = createTemporaryObject(vmComponent, testCase, {profileImportOutcome: "succeeded"})
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        const summary = find(status, "profileImportSummary")
        compare(summary.text, "Profile is up to date.")
        vm.profileImportOutcome = "failed"
        vm.profileImportSaveFailed = true
        verify(summary.text.indexOf("previous profile is unchanged") >= 0)
    }
    function test_partialImportOffersReadableIssueDetails() {
        const vm = createTemporaryObject(vmComponent, testCase, {
            profileImportOutcome: "partial", profileImportAdded: 1,
            profileImportIssues: [{path: "C:/source/broken.perf", message: "Incomplete file", retryable: true}]
        })
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        const button = find(status, "profileImportIssuesButton")
        verify(button.visible)
        mouseClick(button)
        verify(find(status, "profileImportIssueDetails").visible)
        compare(button.text, "1 source file could not be imported")
    }
    function test_staleOutcomeIsHidden() {
        const vm = createTemporaryObject(vmComponent, testCase, {profileImportOutcome: "stale"})
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        compare(status.visible, false)
        vm.profileImportOutcome = "succeeded"
        compare(status.visible, true)
    }
    function test_failedOutcomeShowsItsOwnCopy() {
        const vm = createTemporaryObject(vmComponent, testCase, {profileImportOutcome: "failed"})
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        compare(find(status, "profileImportSummary").text, "Profile refresh failed.")
    }
    function test_countsReadCorrectlyInSingularAndPlural() {
        const vm = createTemporaryObject(vmComponent, testCase, {
            profileImportOutcome: "partial", profileImportAdded: 1, profileImportEnriched: 3,
            profileImportIssues: [{path: "a", message: "x"}, {path: "b", message: "y"}]
        })
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        compare(find(status, "profileImportSummary").text, "Added 1 run; updated 3 runs.")
        compare(find(status, "profileImportIssuesButton").text, "2 source files could not be imported")
    }
    function test_longIssuePathsWrapInsteadOfOverflowingHorizontally() {
        const longPath = "C:/Users/Someone/Very/Deeply/Nested/Directory/Structure/" + "segment/".repeat(12) + "broken.perf"
        const vm = createTemporaryObject(vmComponent, testCase, {
            profileImportOutcome: "partial",
            profileImportIssues: [{path: longPath, message: "Incomplete file that could not be decoded", retryable: true}]
        })
        const status = createTemporaryObject(statusComponent, testCase, {sessionVm: vm})
        mouseClick(find(status, "profileImportIssuesButton"))
        const label = find(status, "profileImportIssueLabel")
        tryVerify(() => label.width > 0)
        verify(label.width <= status.width)
        verify(label.lineCount > 1)
        verify(label.contentWidth <= label.width)
    }
}
