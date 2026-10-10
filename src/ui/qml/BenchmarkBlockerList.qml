import QtQuick
import QtQuick.Controls

// The scenarios still short of the next tier, each with a bar showing how far it
// has come from the highest threshold it already meets toward the score the next
// tier needs, so the scenarios can be compared at a glance. The exact
// "PB / needed" numbers stay alongside each bar.
Column {
    id: root

    property var blockers: []
    // The next tier's name and readable colour, for the bar column heading/fill.
    property string nextTierName: ""
    property string nextTierColor: ""

    readonly property real columnSpacing: 12
    readonly property real barMinimumWidth: 80

    spacing: 4

    Label {
        id: fontProbe
        visible: false
        text: "0"
    }
    FontMetrics {
        id: metrics
        font: fontProbe.font
    }

    function rowLabel(item) {
        if (!item) return ""
        if (item.kind === "group") return String(item.groupName)
        var group = String(item.groupName || "")
        return group.length > 0 ? qsTr("%1 (%2)").arg(item.scenarioName).arg(group) : String(item.scenarioName)
    }

    function valueText(item) {
        if (!item) return ""
        return qsTr("%1 / %2").arg(item.currentText).arg(item.requiredText)
    }

    readonly property real _valueWidth: {
        var widest = metrics.advanceWidth(qsTr("PB / needed"))
        for (var i = 0; i < root.blockers.length; ++i)
            widest = Math.max(widest, metrics.advanceWidth(valueText(root.blockers[i])))
        return Math.ceil(widest) + 2
    }
    readonly property real _nameWidth: {
        var widest = metrics.advanceWidth(qsTr("Scenario"))
        for (var i = 0; i < root.blockers.length; ++i)
            widest = Math.max(widest, metrics.advanceWidth(rowLabel(root.blockers[i])))
        var available = root.width - root._valueWidth - root.barMinimumWidth - 2 * root.columnSpacing
        return Math.max(60, Math.min(Math.ceil(widest) + 2, available, root.width * 0.45))
    }
    readonly property real _barX: _nameWidth + columnSpacing
    // Capped so the numbers stay close to their bar on wide windows.
    readonly property real barMaximumWidth: 420
    readonly property real _barWidth: Math.max(0, Math.min(barMaximumWidth,
                                                           width - _nameWidth - _valueWidth - 2 * columnSpacing))
    readonly property real _valueX: _barX + _barWidth + columnSpacing

    Label {
        width: root.width
        visible: root.blockers.length === 0
        text: qsTr("No blockers for the next tier.")
        color: root.palette.placeholderText
    }

    // Column headings.
    Item {
        objectName: "blockerHeader"
        visible: root.blockers.length > 0
        width: root.width
        height: visible ? headerName.implicitHeight : 0

        Label {
            id: headerName
            width: root._nameWidth
            elide: Text.ElideRight
            text: qsTr("Scenario")
            color: root.palette.placeholderText
        }
        Label {
            x: root._barX
            width: root._barWidth
            elide: Text.ElideRight
            text: root.nextTierName.length > 0 ? qsTr("Progress toward %1").arg(root.nextTierName)
                                               : qsTr("Progress toward the next tier")
            color: root.palette.placeholderText
        }
        Label {
            x: root._valueX
            width: root._valueWidth
            horizontalAlignment: Text.AlignRight
            text: qsTr("PB / needed")
            color: root.palette.placeholderText
        }
    }

    Repeater {
        model: root.blockers

        delegate: Item {
            id: blockerRow
            required property var modelData
            required property int index

            objectName: "blocker_" + index
            // One readable line for tests and assistive tech.
            readonly property string text: modelData.kind === "group"
                  ? qsTr("%1: %2 (%3)").arg(modelData.groupName).arg(modelData.currentText).arg(modelData.requiredText)
                  : qsTr("%1: %2 / %3").arg(root.rowLabel(modelData))
                        .arg(modelData.currentText).arg(modelData.requiredText)
            readonly property real progress: {
                var value = Number(modelData.progress)
                return isNaN(value) ? 0 : Math.max(0, Math.min(1, value))
            }

            width: root.width
            height: Math.max(nameLabel.implicitHeight, 16)
            Accessible.role: Accessible.StaticText
            Accessible.name: text
            Accessible.description: String(modelData.progressDescription || "")

            Label {
                id: nameLabel
                width: root._nameWidth
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: root.rowLabel(blockerRow.modelData)
            }

            Rectangle {
                id: track
                objectName: "blockerBar_" + blockerRow.index
                x: root._barX
                width: root._barWidth
                height: 8
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: Qt.alpha(root.palette.text, 0.12)

                Rectangle {
                    objectName: "blockerFill_" + blockerRow.index
                    width: Math.round(track.width * blockerRow.progress)
                    height: parent.height
                    radius: parent.radius
                    visible: width > 0
                    color: String(blockerRow.modelData.progressColor || root.nextTierColor || "").length > 0
                           ? (blockerRow.modelData.progressColor || root.nextTierColor) : root.palette.accent
                }

                HoverHandler { id: barHover }
                ToolTip.visible: barHover.hovered && ToolTip.text.length > 0
                ToolTip.delay: 300
                ToolTip.text: String(blockerRow.modelData.progressDescription || "")
            }

            Label {
                x: root._valueX
                width: root._valueWidth
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: root.valueText(blockerRow.modelData)
            }
        }
    }
}
