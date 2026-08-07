import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root
    required property var graphVm
    required property var columnVisibility

    Layout.fillWidth: true
    Layout.fillHeight: true

    background: Rectangle {
        radius: 12
        color: "#1E1E1E"
        border.color: "#2A2A2A"
    }
    
    readonly property var visibleColumns: {
        const cols = root.graphVm.plottableColumns
        const result = []
        for (let i = 0; i < cols.length; i++) {
            result.push(!!root.columnVisibility[root.graphVm.columnName(cols[i]).toLowerCase()])
        }
        return result
    }

    GraphCanvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: 1
        graphVm: root.graphVm
        visibleColumns: root.visibleColumns
    }

    MouseArea {
        id: hoverArea
        anchors.fill: canvas
        hoverEnabled: true
        acceptedButtons: Qt.NoButton

        property var hoverInfo: ({valid: false})

        onPositionChanged: mouse => hoverInfo = canvas.nearestPoint(mouse.x, mouse.y)
        onExited: hoverInfo = {valid: false}
    }

    Text {
        id: tooltipText
        visible: hoverArea.hoverInfo.valid === true
        color: "white"
        font.bold: true
        text: hoverArea.hoverInfo.valid
            ? `(${hoverArea.hoverInfo.time.toFixed(2)}, ${hoverArea.hoverInfo.value.toFixed(2)})`
            : ""
        x: Math.min(hoverArea.mouseX + 8, root.width - implicitWidth - 4)
        y: Math.max(hoverArea.mouseY - implicitHeight - 8, 0)
    }
}
