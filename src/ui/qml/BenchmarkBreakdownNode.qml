import QtQuick
import QtQuick.Controls

// One node of the breakdown table and, recursively, its visible children. A group
// (category / subcategory / Uncategorized) renders as a full-width heading row with
// an expansion toggle; a scenario renders its values into the shared columns the
// owning BenchmarkBreakdownView (`table`) measured for the whole tree.
Column {
    id: nodeRoot

    property var node
    property var trackingVm
    property var table
    property int depth: 0

    readonly property string nodeId: node ? String(node.nodeId) : ""
    readonly property bool isScenario: node !== undefined && node !== null && node.kind === "scenario"
    readonly property bool hasChildren: node !== undefined && node !== null && node.children !== undefined
                                        && node.children.length > 0

    // Groups indent by depth; a scenario's name lines up with its group's name.
    // The geometry lives on the table so its column measurement agrees with it.
    readonly property real toggleWidth: table ? table.toggleWidth : 22
    readonly property real swatchWidth: table ? table.swatchWidth : 10
    readonly property real indentStep: table ? table.indentStep : 16
    readonly property real groupNameX: table ? table.groupNameIndent(depth) : 0
    readonly property real scenarioNameX: table ? table.scenarioNameIndent(depth) : 0

    objectName: "breakdownNode_" + nodeId
    width: parent ? parent.width : 0
    spacing: 2

    function _visibleChildren() {
        return (node && node.expanded && node.children) ? node.children : []
    }

    function _scenarioCountText(count) {
        return count === 1 ? qsTr("1 scenario") : qsTr("%1 scenarios").arg(count)
    }

    // --- group heading -----------------------------------------------------
    // Anchor/x-positioned rather than a RowLayout so the toggle has a real size and
    // position on the first frame - the QML tests click it without waiting for a
    // layout polish.
    Item {
        id: groupRow
        visible: !nodeRoot.isScenario
        width: nodeRoot.width
        implicitHeight: visible ? Math.max(toggleButton.implicitHeight, groupName.implicitHeight) + 4 : 0
        height: implicitHeight

        Button {
            id: toggleButton
            objectName: "breakdownToggle_" + nodeRoot.nodeId
            x: nodeRoot.depth * nodeRoot.indentStep
            anchors.verticalCenter: parent.verticalCenter
            visible: nodeRoot.hasChildren
            width: nodeRoot.toggleWidth
            flat: true
            padding: 2
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            text: (nodeRoot.node && nodeRoot.node.expanded) ? "▾" : "▸"
            Accessible.name: qsTr("Toggle %1").arg(nodeRoot.node ? nodeRoot.node.name : "")
            onClicked: if (nodeRoot.trackingVm && nodeRoot.node)
                           nodeRoot.trackingVm.setExpanded(nodeRoot.node.nodeId, !nodeRoot.node.expanded)
        }

        Rectangle {
            id: swatch
            x: nodeRoot.depth * nodeRoot.indentStep + nodeRoot.toggleWidth + 6
            anchors.verticalCenter: parent.verticalCenter
            width: nodeRoot.swatchWidth
            height: nodeRoot.swatchWidth
            radius: 2
            // Uncategorized has no colour of its own and shows no swatch.
            visible: nodeRoot.node !== undefined && nodeRoot.node !== null && nodeRoot.node.kind !== "uncategorized"
            color: visible ? nodeRoot.node.color : "transparent"
            border.color: palette.mid
        }

        Row {
            x: nodeRoot.groupNameX
            width: Math.max(0, nodeRoot.width - x)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            clip: true

            Label {
                id: groupName
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_name"
                text: nodeRoot.node ? nodeRoot.node.name : ""
                font.bold: true
            }
            Label {
                // Uncategorized is not ranked as a group, so it has no "highest" tier.
                visible: nodeRoot.node !== undefined && nodeRoot.node !== null
                         && nodeRoot.node.kind !== "uncategorized"
                text: qsTr("highest")
                color: palette.placeholderText
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_highestTier"
                visible: nodeRoot.node !== undefined && nodeRoot.node !== null
                         && nodeRoot.node.kind !== "uncategorized"
                text: nodeRoot.node ? String(nodeRoot.node.highestTierText || "") : ""
                color: (nodeRoot.node && String(nodeRoot.node.highestTierColor || "").length > 0)
                       ? nodeRoot.node.highestTierColor : palette.placeholderText
            }
            Label {
                objectName: "breakdownNode_" + nodeRoot.nodeId + "_summary"
                color: palette.placeholderText
                text: {
                    if (!nodeRoot.node) return ""
                    var count = nodeRoot.node.scenarioCount !== undefined ? nodeRoot.node.scenarioCount
                                                                          : nodeRoot.node.childCount
                    var parts = [nodeRoot._scenarioCountText(count)]
                    if (nodeRoot.node.nextTierBlocker === true) parts.push(qsTr("blocks next tier"))
                    return (nodeRoot.node.kind !== "uncategorized" ? "· " : "") + parts.join(" · ")
                }
            }
        }
    }

    // --- scenario row --------------------------------------------------------
    Item {
        id: scenarioRow
        objectName: nodeRoot.isScenario ? "breakdownNode_" + nodeRoot.nodeId + "_metricsRow" : ""
        visible: nodeRoot.isScenario && nodeRoot.table !== undefined && nodeRoot.table !== null
        width: nodeRoot.width
        implicitHeight: visible ? scenarioName.implicitHeight + 6 : 0
        height: implicitHeight

        readonly property var cols: nodeRoot.table ? nodeRoot.table.columns : null
        readonly property string statusText: nodeRoot.table && nodeRoot.isScenario
                                             ? nodeRoot.table.scenarioStatusText(nodeRoot.node) : ""
        readonly property bool abnormal: nodeRoot.isScenario && String(nodeRoot.node.matchStateText || "").length > 0

        Label {
            id: scenarioName
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_name"
            x: nodeRoot.scenarioNameX
            y: 3
            width: scenarioRow.cols ? Math.max(0, scenarioRow.cols.nameWidth - x) : 0
            elide: Text.ElideRight
            text: nodeRoot.isScenario ? nodeRoot.node.name : ""
        }
        Label {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_personalBest"
            elide: Text.ElideRight
            x: scenarioRow.cols ? scenarioRow.cols.pb.x : 0
            y: 3
            width: scenarioRow.cols ? scenarioRow.cols.pb.width : 0
            horizontalAlignment: Text.AlignRight
            text: nodeRoot.isScenario ? nodeRoot.node.personalBestText : ""
            Accessible.name: qsTr("PB %1").arg(text)
        }
        Label {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_recentAverage"
            elide: Text.ElideRight
            x: scenarioRow.cols ? scenarioRow.cols.recent.x : 0
            y: 3
            width: scenarioRow.cols ? scenarioRow.cols.recent.width : 0
            horizontalAlignment: Text.AlignRight
            text: nodeRoot.table && nodeRoot.isScenario ? nodeRoot.table.scenarioRecentText(nodeRoot.node) : ""
            Accessible.name: qsTr("Recent %1").arg(text)
        }
        Label {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_attainedTier"
            elide: Text.ElideRight
            x: scenarioRow.cols ? scenarioRow.cols.rank.x : 0
            y: 3
            width: scenarioRow.cols ? scenarioRow.cols.rank.width : 0
            text: nodeRoot.isScenario ? nodeRoot.node.attainedTierText : ""
            color: (nodeRoot.isScenario && String(nodeRoot.node.attainedTierColor || "").length > 0)
                   ? nodeRoot.node.attainedTierColor : palette.placeholderText
            Accessible.name: qsTr("Rank %1").arg(text)
        }
        Row {
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_nextThreshold"
            x: scenarioRow.cols ? scenarioRow.cols.next.x : 0
            y: 3
            width: scenarioRow.cols ? scenarioRow.cols.next.width : 0
            layoutDirection: Qt.RightToLeft
            spacing: 4
            clip: true
            // Read by tests and assistive tech as one value.
            readonly property string text: nodeRoot.table && nodeRoot.isScenario
                                           ? nodeRoot.table.scenarioNextText(nodeRoot.node) : ""
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Next tier %1").arg(text)

            Label {
                text: nodeRoot.isScenario ? nodeRoot.node.nextThresholdText : ""
                color: String(nodeRoot.isScenario ? nodeRoot.node.nextTierText || "" : "").length > 0
                       ? palette.text : palette.placeholderText
            }
            Label {
                visible: text.length > 0
                text: nodeRoot.isScenario ? String(nodeRoot.node.nextTierText || "") : ""
                color: (nodeRoot.isScenario && String(nodeRoot.node.nextTierColor || "").length > 0)
                       ? nodeRoot.node.nextTierColor : palette.text
            }
        }
        Label {
            id: statusLabel
            objectName: "breakdownNode_" + nodeRoot.nodeId + "_matchState"
            x: scenarioRow.cols ? scenarioRow.cols.status.x : 0
            y: 3
            width: scenarioRow.cols ? scenarioRow.cols.status.width : 0
            elide: Text.ElideRight
            text: scenarioRow.statusText
            color: scenarioRow.abnormal ? "#FFB74D" : palette.placeholderText
            Accessible.name: text
            Accessible.description: nodeRoot.isScenario ? String(nodeRoot.node.matchStateHint || "") : ""

            HoverHandler { id: statusHover }
            ToolTip.visible: statusHover.hovered && ToolTip.text.length > 0
            ToolTip.delay: 400
            ToolTip.text: {
                if (!nodeRoot.isScenario) return ""
                var hint = String(nodeRoot.node.matchStateHint || "")
                return hint.length > 0 ? hint : (statusLabel.truncated ? statusLabel.text : "")
            }
        }
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
                item.table = Qt.binding(() => nodeRoot.table)
                item.depth = nodeRoot.depth + 1
            }
        }
    }
}
