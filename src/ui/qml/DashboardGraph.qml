import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtGraphs

Frame {
    id: root
    required property var graphVm

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.horizontalStretchFactor: 3

    background: Rectangle {
        radius: 12
        color: "#1E1E1E"
        border.color: "#2A2A2A"
    }

    GraphsView {
        anchors.fill: parent
        anchors.margins: 1
        axisX: ValueAxis {
            min: root.graphVm.xMin; max: root.graphVm.xMax
        }
        axisY: ValueAxis {
            min: root.graphVm.yMin; max: root.graphVm.yMax; subTickCount: 4
        }
        LineFromModel {
            line_model: root.graphVm
            xIndex: 0
            yIndex: 1
            color: "#009600"
            width: 3
            pointDelegate: GraphHoverPointDelegate {}
        }

        LineFromModel {
            line_model: root.graphVm
            xIndex: 0
            yIndex: 2
            color: "cyan"
            width: 3
            pointDelegate: GraphHoverPointDelegate {}
        }
    }
}
