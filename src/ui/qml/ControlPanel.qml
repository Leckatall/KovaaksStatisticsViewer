pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The per-run graph's line toggles, laid out as a wrapping row inside the graph panel they
// control: one chip per enabled series plus "Configure...". Each chip's swatch is drawn in the
// line's exact colour at full opacity (filled while the line is shown, outlined while hidden),
// and its label uses the normal text colour so it stays readable on every series colour.
Item {
    id: root

    required property var graphVm
    required property var visualSettings

    signal configureLinesRequested

    function seriesFor(id) {
        const series = root.graphVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].id === id) return series[i]
        }
        return null
    }

    // The chips wrap to whatever width the panel gives them, so only the narrowest useful width is
    // reported; reporting the one-row width would make the graph column claim it from the layout.
    implicitWidth: configureButton.implicitWidth
    implicitHeight: flow.implicitHeight

    Flow {
        id: flow

        width: root.width
        spacing: 2

        Repeater {
            model: root.graphVm.enabledSeriesIds

            CheckBox {
                id: toggle

                required property string modelData
                readonly property var series: root.seriesFor(modelData)
                readonly property color lineColor: series ? series.color : toggle.palette.text

                checked: root.visualSettings.isSeriesVisible(modelData)
                objectName: "seriesVisibilityCheckBox_" + modelData
                text: series ? series.name : modelData
                spacing: 5
                topPadding: 3
                bottomPadding: 3
                leftPadding: 5
                rightPadding: 7
                Accessible.name: qsTr("Show %1 line").arg(toggle.text)

                indicator: Rectangle {
                    objectName: "seriesSwatch_" + toggle.modelData
                    x: toggle.leftPadding
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12
                    height: 12
                    radius: 3
                    color: toggle.checked ? toggle.lineColor : "transparent"
                    border.width: 2
                    border.color: toggle.lineColor
                }
                contentItem: Label {
                    leftPadding: toggle.indicator.width + toggle.spacing
                    text: toggle.text
                    color: toggle.checked ? toggle.palette.text : Qt.alpha(toggle.palette.text, 0.55)
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    color: toggle.hovered ? Qt.alpha(toggle.palette.text, 0.08) : "transparent"
                    border.width: toggle.visualFocus ? 1 : 0
                    border.color: toggle.palette.highlight
                }

                onToggled: root.visualSettings.setSeriesVisible(modelData, checked)
            }
        }
        Label {
            objectName: "noEnabledGraphLinesLabel"
            height: configureButton.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("No graph lines enabled")
            visible: root.graphVm.enabledSeriesIds.length === 0
        }
        Button {
            id: configureButton

            objectName: "configureGraphLinesButton"
            text: qsTr("Configure...")
            topPadding: 3
            bottomPadding: 3

            onClicked: root.configureLinesRequested()
        }
    }
}
