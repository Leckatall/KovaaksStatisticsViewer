import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

Flow {
    id: list

    property var groups: []
    property string focusedGroupId: ""
    property string hoverDropGroupId: ""
    // (categoryId, name) => bool
    property var submitSubcategory: null

    signal groupFocused(string groupId)
    // In this list's coordinates.
    signal contextRequested(string groupId, real x, real y)
    signal renameRequested(string groupId, string name)

    objectName: "groupList"
    spacing: 4
    visible: groupRepeater.count > 0

    function delegateFor(groupId) {
        for (let i = 0; i < groupRepeater.count; ++i) {
            const item = groupRepeater.itemAt(i)
            if (item && item.groupId === groupId) return item
        }
        return null
    }

    function focusButton(groupId) {
        const item = list.delegateFor(groupId)
        return item ? item.focusButton : null
    }

    function beginRename(groupId) {
        const item = list.delegateFor(groupId)
        if (!item) return false
        item.renaming = true
        item.renameField.text = item.modelData.name
        item.renameField.forceActiveFocus()
        return true
    }

    function beginSubcategoryCreation(groupId) {
        const item = list.delegateFor(groupId)
        if (!item) return false
        item.addingSubcategory = true
        item.subcategoryField.forceActiveFocus()
        return true
    }

    function groupAtScene(scenePoint) {
        for (let i = 0; i < groupRepeater.count; ++i) {
            const item = groupRepeater.itemAt(i)
            const button = item ? item.focusButton : null
            if (!button || !button.visible) continue
            const local = button.mapFromItem(null, scenePoint.x, scenePoint.y)
            if (local.x >= 0 && local.y >= 0 && local.x < button.width && local.y < button.height) return item.groupId
        }
        return null
    }

    Repeater {
        id: groupRepeater
        model: list.groups
        delegate: Row {
            id: groupItem
            required property var modelData
            readonly property string groupId: groupItem.modelData.id
            readonly property alias focusButton: groupButton
            property bool addingSubcategory: false
            property bool renaming: false
            readonly property alias subcategoryField: emptySubcategoryName
            readonly property alias renameField: emptyGroupRenameField
            Button {
                id: groupButton
                objectName: "groupListItem_" + groupItem.groupId
                flat: true
                visible: !groupItem.renaming
                activeFocusOnTab: true
                text: (groupItem.modelData.kind === "subcategory" ? "↳ " : "")
                    + (groupItem.modelData.name !== "" ? groupItem.modelData.name : qsTr("(unnamed)"))
                    + " " + qsTr("(empty)")
                    + ((groupItem.modelData.issues || []).length > 0 ? " ⚠" : "")
                Accessible.name: text
                Accessible.description: (groupItem.modelData.issues || []).join("; ")
                ToolTip.visible: hovered && (groupItem.modelData.issues || []).length > 0
                ToolTip.text: Accessible.description
                highlighted: list.focusedGroupId === groupItem.groupId
                             || list.hoverDropGroupId === groupItem.groupId
                onActiveFocusChanged: if (activeFocus) list.groupFocused(groupItem.groupId)
                onClicked: list.groupFocused(groupItem.groupId)
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        const p = groupButton.mapToItem(list, 0, groupButton.height)
                        list.contextRequested(groupItem.groupId, p.x, p.y)
                    }
                }
            }
            TooltipToolButton {
                visible: groupItem.modelData.kind === "category" && !groupItem.addingSubcategory && !groupItem.renaming
                text: "+"
                Accessible.name: qsTr("Add subcategory to %1").arg(groupItem.modelData.name)
                ToolTip.text: Accessible.name
                onClicked: {
                    groupItem.addingSubcategory = true
                    emptySubcategoryName.forceActiveFocus()
                }
            }
            CreationField {
                id: emptySubcategoryName
                visible: groupItem.addingSubcategory
                width: 130
                placeholderText: qsTr("Subcategory name")
                submit: name => list.submitSubcategory !== null && list.submitSubcategory(groupItem.groupId, name)
                onDone: groupItem.addingSubcategory = false
            }
            TextField {
                id: emptyGroupRenameField
                objectName: "emptyGroupRenameField_" + groupItem.groupId
                visible: groupItem.renaming
                width: 140
                Accessible.name: qsTr("Group name")
                onAccepted: {
                    list.renameRequested(groupItem.groupId, text)
                    groupItem.renaming = false
                }
                Keys.onEscapePressed: groupItem.renaming = false
            }
        }
    }
}
