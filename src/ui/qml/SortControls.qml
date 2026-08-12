import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 0

    property alias comboObjectName: sortCombo.objectName
    property alias buttonObjectName: sortDirectionButton.objectName
    property var options
    property int sortField: 0
    property bool sortAscending: false

    signal sortRequested(int field, bool ascending)

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: "Sort By:"
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 4

        ComboBox {
            id: sortCombo
            currentIndex: root.sortField
            model: root.options
            onActivated: index => root.sortRequested(index, root.sortAscending)
        }

        ToolButton {
            id: sortDirectionButton
            text: root.sortAscending ? "▲" : "▼"
            onClicked: root.sortRequested(root.sortField, !root.sortAscending)
        }
    }
}
