pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer
import "GraphCatalog.js" as GraphCatalog

// A graph's capture actions: Copy and Save use the default capture options, Preview opens the
// adjustable export preview. `canvas` is null for targets drawn without one (the benchmark table).
Control {
    id: root

    property var captureVm: null
    // One of run, dailyPlaytime, scenarioHistory, benchmarkRank, benchmarkRecentRank, benchmarkPlaytime, benchmarkTable.
    property string targetKey: ""
    property string targetName: root.targetKey === "benchmarkTable" ? qsTr("Benchmark table")
                                                                     : GraphCatalog.nameOf(root.targetKey)
    property var canvas: null

    visible: root.captureVm !== null
    padding: 3
    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("%1 image capture").arg(root.targetName)

    background: Rectangle {
        radius: 4
        color: "transparent"
        border.color: Qt.alpha(root.palette.buttonText, 0.2)
    }

    component CaptureButton: ChromeIconButton {
        id: button

        padding: 2
        focusPolicy: Qt.StrongFocus
        Accessible.name: button.text
        ToolTip.text: button.text
    }

    component Divider: Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 1
        height: 14
        color: Qt.alpha(root.palette.buttonText, 0.2)
    }

    contentItem: Row {
        spacing: 2

        CaptureButton {
            objectName: "captureCopy_" + root.targetKey
            icon.source: "../icons/copy.svg"
            text: qsTr("Copy %1 image to clipboard").arg(root.targetName)
            onClicked: root.captureVm.quickCopy(root.targetKey, root.canvas)
        }
        Divider {}
        CaptureButton {
            objectName: "captureSave_" + root.targetKey
            icon.source: "../icons/save.svg"
            text: qsTr("Save %1 image…").arg(root.targetName)
            onClicked: root.captureVm.quickSave(root.targetKey, root.canvas)
        }
        Divider {}
        CaptureButton {
            id: preview
            objectName: "capturePreview_" + root.targetKey
            icon.source: "../icons/eye.svg"
            text: qsTr("Preview %1 image…").arg(root.targetName)
            onClicked: {
                root.openedPreview = true
                root.captureVm.openPreview(root.targetKey, root.canvas)
            }
        }
    }

    // Only the instance that opened the preview takes focus back; other graphs' buttons stay put.
    property bool openedPreview: false

    Connections {
        target: root.captureVm
        ignoreUnknownSignals: true

        function onPreviewClosed() {
            if (!root.openedPreview) return
            root.openedPreview = false
            preview.forceActiveFocus(Qt.TabFocusReason)
        }
    }
}
