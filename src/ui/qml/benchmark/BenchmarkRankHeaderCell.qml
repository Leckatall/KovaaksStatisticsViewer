import QtQuick
import QtQuick.Controls
import KovaaksStatsViewer

FocusScope {
    id: headerCell
    required property var model
    required property int column
    property Item table: null
    readonly property string tierId: headerCell.model.tierId !== undefined ? headerCell.model.tierId : ""
    readonly property string headerName: headerCell.model.display !== undefined ? headerCell.model.display : ""
    readonly property var headerIssues: headerCell.model.issues || []
    readonly property var headerColor: headerCell.model.headerColor !== undefined ? headerCell.model.headerColor : "transparent"
    property bool editingName: false
    objectName: headerCell.tierId !== "" ? "rankHeader_" + headerCell.tierId : ""
    implicitWidth: table.thresholdWidth
    implicitHeight: table.headerHeight
    activeFocusOnTab: headerCell.tierId !== ""
    Accessible.role: Accessible.ColumnHeader
    Accessible.name: headerCell.headerName !== "" ? headerCell.headerName : qsTr("Unnamed rank")
    Accessible.description: headerCell.headerIssues.join("; ")
    onActiveFocusChanged: if (activeFocus) {
        table.focusedTierId = headerCell.tierId
        table.headerFocused = true
    }

    function beginRename() {
        if (headerCell.tierId !== "") headerCell.editingName = true
    }

    // While the name editor is open, its keys (including Paste and Undo) stay local.
    Keys.onPressed: (event) => {
        if (headerCell.editingName) {
            event.accepted = false
        } else if (event.key === Qt.Key_F2 || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            headerCell.beginRename()
            event.accepted = true
        } else {
            table.handleCommandKey(event)
        }
    }

    TapHandler {
        onTapped: headerCell.forceActiveFocus()
        onDoubleTapped: headerCell.beginRename()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            headerCell.forceActiveFocus()
            const point = headerCell.mapToItem(table, eventPoint.position.x, eventPoint.position.y)
            table.rankContextRequested(headerCell.tierId, point.x, point.y)
        }
    }

    GridHeaderCell {
        anchors.fill: parent
        focused: headerCell.activeFocus
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 4
        color: headerCell.headerColor
    }
    TableSwatch {
        objectName: "rankSwatch_" + headerCell.tierId
        visible: headerCell.tierId !== "" && !headerCell.editingName
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        swatchColor: headerCell.headerColor
        Accessible.name: qsTr("Colour of rank %1").arg(headerCell.Accessible.name)
        onClicked: table.colorSwatchRequested("tier", headerCell.tierId, headerCell.headerColor)
    }
    Loader {
        anchors.fill: parent
        anchors.margins: 2
        active: headerCell.editingName
        sourceComponent: TextField {
            id: rankNameEditor
            objectName: "rankHeaderEditor"
            property bool cancelled: false
            text: headerCell.headerName
            Accessible.name: qsTr("Rank name")
            Component.onCompleted: {
                rankNameEditor.selectAll()
                rankNameEditor.forceActiveFocus()
            }
            Keys.onEscapePressed: {
                rankNameEditor.cancelled = true
                headerCell.editingName = false
                headerCell.forceActiveFocus()
            }
            // Return keeps focus on the header; losing focus elsewhere must not steal it back.
            onEditingFinished: {
                const returnFocus = rankNameEditor.activeFocus
                if (!rankNameEditor.cancelled && rankNameEditor.text !== headerCell.headerName)
                    table.manager.renameTier(headerCell.tierId, rankNameEditor.text)
                headerCell.editingName = false
                if (returnFocus) headerCell.forceActiveFocus()
            }
        }
    }
    Label {
        anchors.fill: parent
        anchors.margins: 4
        anchors.leftMargin: table.rankHeaderLabelLeftInset
        anchors.rightMargin: table.rankHeaderLabelRightInset
        visible: !headerCell.editingName
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        font.bold: true
        font.italic: headerCell.headerName === ""
        text: table.rankHeaderText(headerCell.headerName, headerCell.headerIssues.length > 0)
    }
}
