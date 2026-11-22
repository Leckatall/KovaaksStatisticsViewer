import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// One choice of an exclusive axis selection; whoever uses it sets ButtonGroup.group. Checked state follows
// `selected`, never a click, so revert and Cancel restore it; the click only reports the key and the
// owner's next `selected` carries the new selection. Controls placed in `header` take their own presses
// and keys, and replace the plain name; anything else on the card chooses it.
ActionButton {
    id: card

    property string choiceKey
    property string choiceName
    property bool selected: false
    property string detailText
    property alias header: headerSlot.data
    default property alias details: detailColumn.data
    readonly property alias focusIndicator: focusRing

    readonly property int markerSize: 14
    readonly property int headerSpacing: 8

    signal chosen(string key)

    text: choiceName
    checkable: false
    checked: selected
    padding: 8
    Accessible.role: Accessible.RadioButton
    Accessible.checkable: true
    Accessible.checked: selected
    Accessible.name: choiceName
    Accessible.description: detailText
    onClicked: if (!selected) chosen(choiceKey)

    contentItem: ColumnLayout {
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            spacing: card.headerSpacing

            Rectangle {
                objectName: "axisChoiceMarker"
                Layout.preferredWidth: card.markerSize
                Layout.preferredHeight: card.markerSize
                Layout.alignment: Qt.AlignVCenter
                radius: width / 2
                color: "transparent"
                border.color: card.selected ? Theme.palette.link : Theme.palette.disabled.text

                Rectangle {
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.palette.link
                    visible: card.selected
                }
            }

            Label {
                objectName: "axisChoiceName"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                visible: headerSlot.children.length === 0
                text: card.choiceName
                wrapMode: Text.WordWrap
            }

            RowLayout {
                id: headerSlot
                Layout.fillWidth: true
                visible: children.length > 0
                spacing: card.headerSpacing
            }
        }

        ColumnLayout {
            id: detailColumn
            Layout.fillWidth: true
            Layout.leftMargin: card.markerSize + card.headerSpacing
            spacing: 2
        }
    }

    background: Rectangle {
        radius: 5
        color: card.down ? Theme.palette.base : card.hovered ? Theme.palette.alternateBase : Theme.palette.mid
        border.color: card.selected ? Theme.palette.link : Theme.palette.midlight
        border.width: card.selected ? 2 : 1

        Rectangle {
            id: focusRing
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.color: Theme.palette.highlight
            border.width: 2
            visible: card.visualFocus
        }
    }
}
