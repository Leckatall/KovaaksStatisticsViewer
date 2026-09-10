import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: nodeRoot

    property var node
    property var trackingVm
    property bool wide: true
    property int depth: 0

    readonly property string nodeId: node ? String(node.nodeId) : ""
    readonly property bool isScenario: node && node.kind === "scenario"
    readonly property bool hasChildren: node && node.children && node.children.length > 0

    objectName: "breakdownNode_" + nodeId
    width: parent ? parent.width : 0
    spacing: 2

    function _visibleChildren() {
        return (node && node.expanded && node.children) ? node.children : []
    }

    // Anchor-positioned rather than a RowLayout so the toggle has a real size and
    // position on the first frame - the QML tests click it without waiting for a
    // layout polish.
    Item {
        id: headerRow
        width: nodeRoot.width
        implicitHeight: Math.max(toggleButton.implicitHeight, labelRow.implicitHeight)
        height: implicitHeight

        Button {
            id: toggleButton
            objectName: "breakdownToggle_" + nodeRoot.nodeId
            anchors.left: parent.left
            anchors.leftMargin: nodeRoot.depth * 16
            anchors.verticalCenter: parent.verticalCenter
            visible: nodeRoot.hasChildren
            width: visible ? implicitWidth : 0
            flat: true
            padding: 2
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            text: (nodeRoot.node && nodeRoot.node.expanded) ? "▾" : "▸"
            Accessible.name: qsTr("Toggle %1").arg(nodeRoot.node ? nodeRoot.node.name : "")
            onClicked: if (nodeRoot.trackingVm && nodeRoot.node)
                           nodeRoot.trackingVm.setExpanded(nodeRoot.node.nodeId, !nodeRoot.node.expanded)
        }

        Row {
            id: labelRow
            anchors.left: toggleButton.right
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
                width: 10
                height: 10
                radius: 2
                visible: !nodeRoot.isScenario && nodeRoot.node && String(nodeRoot.node.color).length > 0
                color: visible ? nodeRoot.node.color : "transparent"
                border.color: nodeRoot.palette.mid
            }

            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_name"
                text: nodeRoot.node ? nodeRoot.node.name : ""
                font.bold: !nodeRoot.isScenario
            }

            Label {
                visible: !nodeRoot.isScenario
                color: nodeRoot.palette.placeholderText
                text: nodeRoot.node
                      ? qsTr("highest %1 · %2 scenarios").arg(nodeRoot.node.highestTierText).arg(nodeRoot.node.childCount)
                      : ""
            }

            Label {
                visible: nodeRoot.node && nodeRoot.node.nextTierBlocker === true
                text: qsTr("· blocks next tier")
                color: nodeRoot.palette.placeholderText
            }

            Loader {
                active: nodeRoot.isScenario && nodeRoot.wide
                visible: active
                sourceComponent: wideMetrics
            }
        }
    }

    Loader {
        width: nodeRoot.width
        active: nodeRoot.isScenario && !nodeRoot.wide
        visible: active
        sourceComponent: narrowMetrics
    }

    // A string `source` breaks the static recursion cycle the QML compiler would
    // otherwise reject; the tree is genuinely self-similar.
    Repeater {
        model: nodeRoot._visibleChildren()
        delegate: Loader {
            required property var modelData
            width: nodeRoot.width
            source: Qt.resolvedUrl("BenchmarkBreakdownNode.qml")
            onLoaded: {
                item.node = Qt.binding(() => modelData)
                item.trackingVm = Qt.binding(() => nodeRoot.trackingVm)
                item.wide = Qt.binding(() => nodeRoot.wide)
                item.depth = nodeRoot.depth + 1
            }
        }
    }

    Component {
        id: wideMetrics
        RowLayout {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_metricsRow"
            spacing: 12

            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_personalBest"
                text: qsTr("PB %1").arg(nodeRoot.node.personalBestText)
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_recentAverage"
                text: qsTr("Recent %1").arg(nodeRoot.node.recentAverageText)
                      + (nodeRoot.node.recentSampleCount > 0 && nodeRoot.node.recentSampleCount < 5
                         ? qsTr(" (%1)").arg(nodeRoot.node.recentSampleCount) : "")
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_attainedTier"
                text: nodeRoot.node.attainedTierText
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_nextThreshold"
                text: nodeRoot.node.nextThresholdText
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_matchState"
                text: nodeRoot.node.matchStateText
            }
        }
    }

    Component {
        id: narrowMetrics
        ColumnLayout {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_twoLineMetrics"
            spacing: 1

            RowLayout {
                spacing: 12
                Label {
                    objectName: "breakdownNode_" + nodeRoot.nodeId + "_personalBest"
                    text: qsTr("PB %1").arg(nodeRoot.node.personalBestText)
                }
                Label {
                    objectName: "breakdownNode_" + nodeRoot.nodeId + "_recentAverage"
                    text: qsTr("Recent %1").arg(nodeRoot.node.recentAverageText)
                }
            }
            RowLayout {
                spacing: 12
                Label {
                    objectName: "breakdownNode_" + nodeRoot.nodeId + "_attainedTier"
                    text: nodeRoot.node.attainedTierText
                }
                Label {
                    objectName: "breakdownNode_" + nodeRoot.nodeId + "_nextThreshold"
                    text: nodeRoot.node.nextThresholdText
                }
                Label {
                    objectName: "breakdownNode_" + nodeRoot.nodeId + "_matchState"
                    text: nodeRoot.node.matchStateText
                }
            }
        }
    }
}
