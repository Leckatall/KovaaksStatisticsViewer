import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    required property var node
    required property bool focused
    required property var kindLabel
    required property var accentColorFn

    default property alias detailContent: detailColumn.children

    signal selected
    signal deleteRequested

    border.color: card.node ? card.accentColorFn(card.node.kind, 0.45) : "transparent"
    border.width: 1
    color: card.node ? card.accentColorFn(card.node.kind, 0.12) : "transparent"
    implicitHeight: cardColumn.implicitHeight + cardColumn.anchors.margins * 2
    implicitWidth: cardColumn.implicitWidth + cardColumn.anchors.margins * 2
    radius: 6

    ColumnLayout {
        id: cardColumn

        anchors.fill: parent
        anchors.margins: 7
        spacing: 6

        Item {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight

            MouseArea {
                anchors.fill: parent

                onClicked: card.selected()
            }
            RowLayout {
                id: headerRow

                anchors.fill: parent

                Label {
                    color: card.node ? card.accentColorFn(card.node.kind, 1) : "transparent"
                    font.bold: true
                    text: card.node ? card.kindLabel(card.node.kind) : ""
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "deleteNodeButton"
                    text: qsTr("Delete")

                    onClicked: card.deleteRequested()
                }
            }
        }
        ColumnLayout {
            id: detailColumn

            Layout.fillWidth: true
            visible: card.focused
        }
    }
}
