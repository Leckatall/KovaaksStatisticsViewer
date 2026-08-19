pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var graphVm
    required property var seriesVisibility

    signal configureLinesRequested

    function seriesFor(id) {
        if (root.graphVm.allSeries) {
            const found = root.graphVm.allSeries.find(s => s.id === id);
            if (found)
                return found;
        }
        return null;
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

                checked: !(root.seriesVisibility.hiddenSeriesIds.indexOf(modelData) === -1)
                objectName: "seriesVisibilityCheckBox_" + modelData
                text: root.seriesFor(modelData).name

                background: Rectangle {
                    anchors.fill: parent
                    color: root.graphVm.columnColor(modelData)
                    opacity: 0.5
                    radius: 5
                }

                onToggled: {
                    root.seriesVisibility.hiddenSeriesIds.push(modelData);
                    console.log(root.seriesVisibility.hiddenSeriesIds);
                }
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
