pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// The per-run graph's line toggles, laid out as a vertical legend beside the plot they control: one
// chip per enabled series, top to bottom in the line-list order, then "Configure...". Each chip's
// swatch is drawn in the line's exact colour at full opacity (filled while the line is shown,
// outlined while hidden), and its label uses the normal text colour so it stays readable on every
// series colour. The chips scroll when the panel is too short for them; "Configure..." stays
// visible below.
Item {
    id: root

    required property var graphVm
    required property var visualSettings

    // Widest a chip may get before its label is elided, so a long custom series name cannot
    // squeeze the plot.
    property real maximumChipWidth: 200

    signal configureLinesRequested

    function seriesFor(id) {
        const series = root.graphVm.allSeries
        for (let i = 0; i < series.length; ++i) {
            if (series[i].id === id) return series[i]
        }
        return null
    }

    // The legend is as wide as its widest chip (plus the scroll bar while it scrolls). Its height
    // is whatever the panel gives it: only one row is reported, so the legend never decides the
    // panel's height.
    implicitWidth: Math.max(chips.width + (chipList.scrollable ? chipScrollBar.width : 0),
                            configureButton.implicitWidth)
    implicitHeight: configureButton.implicitHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        Flickable {
            id: chipList

            readonly property bool scrollable: contentHeight > height

            Layout.fillHeight: true
            Layout.fillWidth: true
            objectName: "graphLineChipList"
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            contentHeight: chips.height
            contentWidth: chips.width
            ScrollBar.vertical: ScrollBar {
                id: chipScrollBar

                objectName: "graphLineChipScrollBar"
                policy: chipList.scrollable ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            Column {
                id: chips

                spacing: 2

                Repeater {
                    model: root.graphVm.enabledSeriesIds

                    CheckBox {
                        id: toggle

                        required property string modelData
                        readonly property var series: root.seriesFor(modelData)
                        readonly property color lineColor: series ? series.color : toggle.palette.text

                        width: Math.min(implicitWidth, root.maximumChipWidth)
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
                            elide: Text.ElideRight
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
                    width: Math.min(implicitWidth, root.maximumChipWidth)
                    topPadding: 3
                    bottomPadding: 3
                    leftPadding: 5
                    text: qsTr("No graph lines enabled")
                    visible: root.graphVm.enabledSeriesIds.length === 0
                    wrapMode: Text.WordWrap
                }
            }
        }
        Button {
            id: configureButton

            objectName: "configureGraphLinesButton"
            text: qsTr("Configure...")
            flat: true
            topPadding: 3
            bottomPadding: 3

            onClicked: root.configureLinesRequested()
        }
    }
}
