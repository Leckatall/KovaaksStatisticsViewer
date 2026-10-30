import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "GraphCatalog.js" as GraphCatalog

// Stands in a graph's place after a quick hide until it is undone or dismissed.
Frame {
    id: root

    objectName: "graphHiddenNotice_" + graphKey

    // Empty while this notice stands in for no graph.
    required property string graphKey
    property var visualSettings: null

    readonly property string graphName: GraphCatalog.nameOf(graphKey)

    visible: graphKey !== "" && visualSettings !== null && visualSettings.hideNoticeGraph === graphKey

    // Deferred so it lands after the closing gear popup has handed focus back.
    onVisibleChanged: if (visible) Qt.callLater(() => undoButton.forceActiveFocus(Qt.TabFocusReason))

    function focusNamed(name) {
        const target = GraphCatalog.findByObjectName(root.Window.window ? root.Window.window.contentItem : null, name)
        if (target) target.forceActiveFocus(Qt.TabFocusReason)
    }

    function undo() {
        const gearName = "graphGear_" + root.graphKey
        root.visualSettings.undoQuickHide()
        root.focusNamed(gearName)
    }

    function dismiss() {
        root.visualSettings.dismissHideNotice()
        root.focusNamed("workspaceTabBar")
    }

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.dismiss()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            objectName: "graphHiddenNoticeTitle"
            Layout.fillWidth: true
            text: qsTr("%1 hidden").arg(root.graphName)
            font.bold: true
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Qt.alpha(root.palette.text, 0.78)
            text: qsTr("Show it again from View → Graphs.")
        }
        RowLayout {
            Button {
                id: undoButton
                objectName: "graphHiddenUndo"
                text: qsTr("Undo")
                onClicked: root.undo()
            }
            Button {
                objectName: "graphHiddenDismiss"
                text: qsTr("Dismiss")
                flat: true
                Accessible.description: qsTr("Dismiss the message. %1 stays hidden.").arg(root.graphName)
                onClicked: root.dismiss()
            }
        }
    }
}
