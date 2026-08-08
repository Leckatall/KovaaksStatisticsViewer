import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

Frame {
    id: root

    required property var columnVisibility
    required property var graphVm
    readonly property var visibleColumns: {
        const cols = root.graphVm.plottableColumns;
        const result = [];
        for (let i = 0; i < cols.length; i++) {
            if (root.columnVisibility[root.graphVm.columnKey(cols[i])]) {
                result.push(cols[i]);
            }
        }
        return result;
    }

    Layout.fillHeight: true
    Layout.fillWidth: true

    background: Rectangle {
        border.color: "#2A2A2A"
        color: "#1E1E1E"
        radius: 12
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            id: scenarioTitleLabel

            Layout.fillWidth: true
            Layout.topMargin: 4
            color: "white"
            font.bold: true
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            objectName: "scenarioTitleLabel"
            text: root.graphVm.scenarioTitle
        }
        GraphCanvasWithTooltip {
            graphVm: root.graphVm
            visibleColumns: root.visibleColumns
        }
    }
}
