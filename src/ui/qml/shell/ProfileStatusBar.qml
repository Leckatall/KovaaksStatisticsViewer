import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Rectangle {
    id: root
    required property var sessionVm
    property int noticeDuration: 4000
    signal detailsRequested()

    readonly property bool busy: root.sessionVm.profileImportBusy === true
    readonly property string phase: root.sessionVm.profileImportPhase || "idle"
    readonly property string importMode: root.sessionVm.profileImportMode || "refresh"
    readonly property string outcome: root.sessionVm.profileImportOutcome || "none"
    readonly property bool saveFailed: root.sessionVm.profileImportSaveFailed === true
    readonly property int added: root.sessionVm.profileImportAdded || 0
    readonly property int enriched: root.sessionVm.profileImportEnriched || 0
    // A source that failed in an earlier import and was skipped unchanged is not news.
    readonly property int newIssueCount: (root.sessionVm.profileImportIssues || [])
        .filter(issue => issue.previouslyFailed !== true).length
    property bool dismissed: false
    property bool noticeActive: false

    readonly property bool warningActive: !root.busy && !root.dismissed
        && (root.saveFailed || root.outcome === "failed" || root.newIssueCount > 0)
    readonly property string mode: root.busy ? "busy"
        : root.warningActive ? "warning"
        : root.noticeActive ? "notice"
        : "hidden"

    visible: root.mode !== "hidden"
    implicitHeight: 26
    color: Qt.darker(palette.window, 1.3)

    function runs(count) { return count === 1 ? qsTr("%1 run").arg(count) : qsTr("%1 runs").arg(count) }

    onBusyChanged: {
        if (root.busy) {
            noticeTimer.stop()
            root.noticeActive = false
            return
        }
        root.dismissed = false
        // Read the VM, not the derived properties above: busy, outcome and counts share one NOTIFY
        // signal, so those bindings may not have re-evaluated yet when this handler runs.
        const vm = root.sessionVm
        const outcome = vm.profileImportOutcome
        const importedSomething = (vm.profileImportAdded || 0) + (vm.profileImportEnriched || 0) > 0
        if ((outcome === "succeeded" || outcome === "partial") && vm.profileImportSaveFailed !== true && importedSomething) {
            root.noticeActive = true
            noticeTimer.restart()
        }
    }

    Timer {
        id: noticeTimer
        interval: root.noticeDuration
        onTriggered: root.noticeActive = false
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: root.palette.mid
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Label {
            objectName: "profileStatusText"
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: 12
            color: root.mode === "warning" ? Theme.warning : root.palette.text
            text: {
                switch (root.mode) {
                case "busy":
                    if (root.phase === "saving") return qsTr("Saving profile…")
                    if (root.sessionVm.profileImportTotal === 0) return qsTr("Scanning sources…")
                    const done = root.sessionVm.profileImportDone
                    const total = root.runs(root.sessionVm.profileImportTotal)
                    return root.importMode === "rebuild"
                        ? qsTr("Rebuilding profile: %1 of %2").arg(done).arg(total)
                        : qsTr("Refreshing profile: %1 of %2").arg(done).arg(total)
                case "warning":
                    if (root.saveFailed) return qsTr("Could not save profile. Your previous profile is unchanged.")
                    if (root.outcome === "failed") return qsTr("Profile refresh failed.")
                    return root.newIssueCount === 1 ? qsTr("1 source file could not be imported")
                        : qsTr("%1 source files could not be imported").arg(root.newIssueCount)
                case "notice":
                    if (root.added === 0) return qsTr("Updated %1").arg(root.runs(root.enriched))
                    return root.enriched > 0
                        ? qsTr("Added %1 · updated %2").arg(root.runs(root.added)).arg(root.enriched)
                        : qsTr("Added %1").arg(root.runs(root.added))
                default:
                    return ""
                }
            }
        }

        ProgressBar {
            objectName: "profileStatusProgressBar"
            visible: root.mode === "busy"
            Layout.preferredWidth: 160
            implicitHeight: 4
            indeterminate: root.phase === "scanning" || root.phase === "saving" || root.sessionVm.profileImportTotal === 0
            value: root.sessionVm.profileImportProgress || 0
        }

        Button {
            objectName: "profileStatusDetailsButton"
            visible: root.mode === "warning"
            flat: true
            text: qsTr("Details…")
            implicitHeight: 22
            verticalPadding: 0
            font.pixelSize: 12
            font.underline: true
            palette.buttonText: root.palette.accent
            onClicked: root.detailsRequested()
        }

        ToolButton {
            objectName: "profileStatusDismissButton"
            visible: root.mode === "warning"
            text: "✕"
            implicitHeight: 22
            implicitWidth: 22
            padding: 0
            font.pixelSize: 11
            Accessible.name: qsTr("Dismiss")
            onClicked: root.dismissed = true
        }
    }
}
