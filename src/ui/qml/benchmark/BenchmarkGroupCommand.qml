pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// Commands leave through signals: a plain-JS test double is copied on assignment to a QObject
// property, so the popup cannot hold a reference to the thing it commands.
Popup {
    id: root
    objectName: "groupCommand"

    // (query, parentCategoryId) => {actions, exactMatch}
    property var optionsProvider: null
    // [{id, name}]
    property var parentCategories: []
    property var scenarioNames: []
    property var actions: []
    property bool exactMatch: false
    property int highlightedIndex: -1

    readonly property int scenarioCount: scenarioNames.length
    // The app ships no translations, so a bare "%n scenario(s)" would show its "(s)" verbatim.
    readonly property string countText: scenarioCount === 1 ? qsTr("1 scenario")
        : qsTr("%n scenarios", "", scenarioCount)
    readonly property string titleText: scenarioCount === 1 ? qsTr("Group 1 scenario")
        : qsTr("Group %n scenarios", "", scenarioCount)
    readonly property string namesText: scenarioNames.join(" · ")
    readonly property string statusText: exactMatch
        ? qsTr("Exact match found. Enter moves to the existing group.") : ""
    property alias queryField: queryInput
    property alias parentPicker: parentCombo
    property alias actionList: actionsView

    readonly property int rowHeight: 36
    readonly property int maxVisibleRows: 6
    readonly property color mutedText: Qt.alpha(palette.text, 0.7)

    signal createRequested(string name, string parentCategoryId)
    signal moveRequested(string targetGroupId)

    popupType: Popup.Item
    modal: true
    focus: true
    padding: 20
    width: parent ? Math.min(560, parent.width - 36) : 560
    height: Math.min(implicitHeight, parent ? parent.height - 36 : implicitHeight)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    function parentName(id) {
        const match = parentCategories.find(category => category.id === id)
        return match ? match.name : ""
    }

    function labelFor(action) {
        if (action.kind === "create") {
            if (action.parentId)
                return qsTr("+ Create subcategory “%1” in %2 and move %3")
                    .arg(action.name).arg(parentName(action.parentId)).arg(countText)
            return qsTr("+ Create category “%1” and move %2").arg(action.name).arg(countText)
        }
        return qsTr("→ Move %1 to %2").arg(countText).arg(action.path)
    }

    function refresh() {
        // currentValue lags currentIndexChanged, and this runs from that handler.
        const parentId = parentPicker.valueAt(parentPicker.currentIndex) ?? ""
        const options = optionsProvider
            ? optionsProvider(queryField.text, parentId) : {actions: [], exactMatch: false}
        actions = options.actions
        exactMatch = options.exactMatch
        highlightedIndex = queryField.text.trim() !== "" && actions.length > 0 ? 0 : -1
    }

    function moveHighlight(delta) {
        if (actions.length === 0 || (delta < 0 && highlightedIndex < 0)) return
        highlightedIndex = Math.max(0, Math.min(actions.length - 1, highlightedIndex + delta))
    }

    function applyHighlighted() {
        if (highlightedIndex >= 0) apply(highlightedIndex)
    }

    function apply(index) {
        const action = actions[index]
        if (action.kind === "create") createRequested(action.name, action.parentId ?? "")
        else moveRequested(action.targetId)
        close()
    }

    onHighlightedIndexChanged: if (highlightedIndex >= 0) actionList.positionViewAtIndex(highlightedIndex, ListView.Contain)
    onAboutToShow: {
        queryField.text = ""
        parentPicker.currentIndex = 0
        refresh()
    }
    onOpened: queryField.forceActiveFocus()

    background: Rectangle {
        radius: 6
        color: root.palette.base
        border.color: root.palette.midlight
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: root.titleText
                font.pixelSize: 19
                font.bold: true
                Layout.fillWidth: true
            }
            Label { text: qsTr("Ctrl+G"); color: root.mutedText; font.pixelSize: 12 }
        }
        Label {
            text: root.namesText
            color: root.mutedText
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label { text: qsTr("Name a new group or search existing groups"); Layout.topMargin: 12 }
        TextField {
            id: queryInput
            objectName: "groupCommandQuery"
            Layout.fillWidth: true
            placeholderText: qsTr("Type a group name…")
            Accessible.name: qsTr("Group name or existing group search")
            selectByMouse: true
            onTextChanged: root.refresh()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down) root.moveHighlight(1)
                else if (event.key === Qt.Key_Up) root.moveHighlight(-1)
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) root.applyHighlighted()
                else return
                event.accepted = true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Create in"); color: root.mutedText }
            ComboBox {
                id: parentCombo
                objectName: "groupCommandParent"
                Layout.fillWidth: true
                model: [{id: "", name: qsTr("Top level category")}].concat(root.parentCategories)
                textRole: "name"
                valueRole: "id"
                Accessible.name: qsTr("Parent category for a new group")
                onCurrentIndexChanged: root.refresh()
                // The ComboBox consumes Return while its list is closed.
                Keys.onPressed: event => {
                    if (parentCombo.popup.visible
                            || (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter)) return
                    root.applyHighlighted()
                    event.accepted = true
                }
            }
        }
        Label {
            text: qsTr("Enter uses the highlighted action. An exact name match uses the existing group.")
            color: root.mutedText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Rectangle { Layout.fillWidth: true; Layout.topMargin: 8; implicitHeight: 1; color: root.palette.midlight }
        Label {
            visible: root.actions.length > 0
            text: qsTr("ACTIONS")
            color: root.mutedText
            font.pixelSize: 11
            font.bold: true
        }
        ListView {
            id: actionsView
            objectName: "groupCommandActions"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(count, root.maxVisibleRows) * root.rowHeight
            Layout.minimumHeight: Math.min(count, 2) * root.rowHeight
            clip: true
            model: root.actions
            boundsBehavior: Flickable.StopAtBounds
            delegate: ItemDelegate {
                id: actionRow
                required property var modelData
                required property int index
                objectName: "groupCommandAction_" + actionRow.index
                width: actionsView.width
                height: root.rowHeight
                text: root.labelFor(actionRow.modelData)
                highlighted: actionRow.index === root.highlightedIndex
                focusPolicy: Qt.NoFocus
                Accessible.name: actionRow.text
                onClicked: root.apply(actionRow.index)
            }
        }
        Label {
            visible: root.statusText !== ""
            text: root.statusText
            color: root.palette.accent
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.palette.midlight }
        Label {
            text: qsTr("↑↓ choose  ·  Enter apply  ·  Tab choose parent  ·  Esc cancel")
            color: root.mutedText
            font.pixelSize: 11
            Layout.fillWidth: true
        }
    }
}
