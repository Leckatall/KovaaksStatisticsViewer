pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var graphVm
    required property var visualSettings

    signal configureLinesRequested

    function seriesFor(id) {
        return root.graphVm.allSeries.find(s => s.id === id) || null
    }

    ColumnLayout {
        Label {
            text: "Lines"
        }
        Repeater {
            model: root.graphVm.enabledSeriesIds

            CheckBox {
                required property string modelData
                readonly property var series: root.seriesFor(modelData)

                checked: root.visualSettings.isSeriesVisible(modelData)
                objectName: "seriesVisibilityCheckBox_" + modelData
                text: series ? series.name : modelData

                background: Rectangle {
                    anchors.fill: parent
                    color: series ? series.color : "transparent"
                    opacity: 0.5
                    radius: 5
                }

                onToggled: root.visualSettings.setSeriesVisible(modelData, checked)
            }
        }
        Label {
            objectName: "noEnabledGraphLinesLabel"
            text: qsTr("No graph lines enabled")
            visible: root.graphVm.enabledSeriesIds.length === 0
        }
        Button {
            objectName: "configureGraphLinesButton"
            text: qsTr("Configure...")

            onClicked: root.configureLinesRequested()
        }
    }
}
