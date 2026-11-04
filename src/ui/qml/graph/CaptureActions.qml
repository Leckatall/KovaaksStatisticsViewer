import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer
import "GraphCatalog.js" as GraphCatalog

// A graph's camera and its export dropdown. The camera copies at the default resolution; the dropdown
// opens the preview. `canvas` is null for targets drawn without one (the benchmark table).
RowLayout {
    id: root

    property var captureVm: null
    // One of run, dailyPlaytime, scenarioHistory, benchmarkRank, benchmarkRecentRank, benchmarkPlaytime, benchmarkTable.
    property string targetKey: ""
    property string targetName: root.targetKey === "benchmarkTable" ? qsTr("Benchmark table")
                                                                     : GraphCatalog.nameOf(root.targetKey)
    property var canvas: null

    readonly property alias menu: exportMenu

    visible: root.captureVm !== null
    spacing: 0

    ChromeIconButton {
        id: camera
        objectName: "captureCopy_" + root.targetKey
        icon.source: "../icons/camera.svg"
        padding: 4
        focusPolicy: Qt.StrongFocus
        text: qsTr("Copy %1 image to clipboard").arg(root.targetName)
        Accessible.name: text
        ToolTip.text: text
        ToolTip.visible: pointerHovered && ToolTip.text !== ""
        onClicked: root.captureVm.quickCopy(root.targetKey, root.canvas)
    }

    ChromeIconButton {
        id: dropdown
        objectName: "captureMenu_" + root.targetKey
        icon.source: "../icons/chevron-down.svg"
        icon.width: 12
        icon.height: 12
        padding: 2
        active: exportMenu.opened
        focusPolicy: Qt.StrongFocus
        text: qsTr("%1 export options").arg(root.targetName)
        Accessible.name: text
        ToolTip.text: text
        ToolTip.visible: pointerHovered && ToolTip.text !== "" && !exportMenu.opened
        onClicked: exportMenu.opened ? exportMenu.close() : exportMenu.open()

        Menu {
            id: exportMenu
            y: dropdown.height

            MenuItem {
                objectName: "captureExport_" + root.targetKey
                text: qsTr("Export image…")
                onTriggered: {
                    root.openedPreview = true
                    root.captureVm.openPreview(root.targetKey, root.canvas)
                }
            }
        }
    }

    // Only the instance that opened the preview takes focus back; other graphs' cameras stay put.
    property bool openedPreview: false

    Connections {
        target: root.captureVm
        ignoreUnknownSignals: true

        function onPreviewClosed() {
            if (!root.openedPreview) return
            root.openedPreview = false
            dropdown.forceActiveFocus(Qt.TabFocusReason)
        }
    }
}
