import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var sessionVm
    readonly property bool busy: root.sessionVm.profileImportBusy === true
    readonly property string phase: root.sessionVm.profileImportPhase || "idle"
    readonly property string mode: root.sessionVm.profileImportMode || "refresh"
    readonly property string outcome: root.sessionVm.profileImportOutcome || "none"
    readonly property var issues: root.sessionVm.profileImportIssues || []
    property bool detailsExpanded: false
    visible: root.busy || (root.outcome !== "none" && root.outcome !== "stale")
    spacing: 4
    onBusyChanged: if (root.busy) root.detailsExpanded = false

    function runs(count) { return count === 1 ? qsTr("%1 run").arg(count) : qsTr("%1 runs").arg(count) }

    Label {
        objectName: "profileBuildProgressLabel"
        Layout.fillWidth: true
        visible: root.busy
        wrapMode: Text.Wrap
        text: root.phase === "saving" ? qsTr("Saving profile...")
            : root.sessionVm.profileImportTotal === 0 ? qsTr("Scanning sources...")
            : root.mode === "rebuild"
                ? qsTr("Rebuilding profile: %1 of %2").arg(root.sessionVm.profileImportDone).arg(root.runs(root.sessionVm.profileImportTotal))
                : qsTr("Refreshing profile: %1 of %2").arg(root.sessionVm.profileImportDone).arg(root.runs(root.sessionVm.profileImportTotal))
    }
    ProgressBar {
        objectName: "profileBuildProgressBar"
        Layout.fillWidth: true
        visible: root.busy
        indeterminate: root.phase === "scanning" || root.phase === "saving" || root.sessionVm.profileImportTotal === 0
        value: root.sessionVm.profileImportProgress || 0
    }
    Label {
        objectName: "profileImportSummary"
        Layout.fillWidth: true
        visible: !root.busy
        wrapMode: Text.Wrap
        text: root.sessionVm.profileImportSaveFailed === true
            ? qsTr("Could not save profile. Your previous profile is unchanged. Refresh to try again.")
            : root.outcome === "failed" ? qsTr("Profile refresh failed.")
            : (root.sessionVm.profileImportAdded || 0) === 0 && (root.sessionVm.profileImportEnriched || 0) === 0
                ? (root.issues.length === 0 ? qsTr("Profile is up to date.") : qsTr("No new runs imported."))
                : qsTr("Added %1; updated %2.").arg(root.runs(root.sessionVm.profileImportAdded || 0)).arg(root.runs(root.sessionVm.profileImportEnriched || 0))
    }
    Button {
        objectName: "profileImportIssuesButton"
        visible: !root.busy && root.issues.length > 0
        text: root.issues.length === 1 ? qsTr("1 source file could not be imported")
            : qsTr("%1 source files could not be imported").arg(root.issues.length)
        checkable: true
        checked: root.detailsExpanded
        onClicked: root.detailsExpanded = checked
    }
    ScrollView {
        id: details
        objectName: "profileImportIssueDetails"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(detailColumn.implicitHeight, 160)
        visible: root.detailsExpanded && !root.busy && root.issues.length > 0
        contentWidth: availableWidth
        Column {
            id: detailColumn
            width: details.availableWidth
            spacing: 4
            Repeater {
                model: root.issues
                Label {
                    objectName: "profileImportIssueLabel"
                    required property var modelData
                    width: detailColumn.width
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    text: qsTr("%1: %2").arg(modelData.path).arg(modelData.message)
                }
            }
        }
    }
}
