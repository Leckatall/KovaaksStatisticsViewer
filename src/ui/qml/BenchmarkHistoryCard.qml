import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var historyModel
    property string namePrefix: "history"

    readonly property bool hasData: root.historyModel !== undefined
                                    && root.historyModel !== null
                                    && root.historyModel.hasData === true
    readonly property string metricName: root.historyModel ? root.historyModel.metricName : ""

    Accessible.role: Accessible.Graphic
    Accessible.name: root.metricName
    Accessible.description: qsTr("%1 history chart").arg(root.metricName)

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            objectName: root.namePrefix + "LineName"
            Layout.fillWidth: true
            font.bold: true
            text: root.metricName
        }

        Label {
            objectName: root.namePrefix + "EmptyLabel"
            Layout.fillWidth: true
            visible: !root.hasData
            wrapMode: Text.WordWrap
            color: root.palette.placeholderText
            text: root.historyModel ? root.historyModel.emptyStateText : ""
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            active: root.hasData
            visible: root.hasData
            sourceComponent: GraphCanvasWithTooltip {
                objectName: root.namePrefix + "Canvas"
                graphVm: root.historyModel
                showSeriesNames: true
                xLabel: qsTr("Date")
            }
        }
    }
}
