import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// "Sort by [field ▾] [▼ Desc]" on one row. The direction button names its current
// state, flips its arrow, and has a tooltip and accessible name.
RowLayout {
    id: root
    spacing: 4

    property alias comboObjectName: sortCombo.objectName
    property alias buttonObjectName: sortDirectionButton.objectName
    property var options
    property int sortField: 0
    property bool sortAscending: false

    readonly property string directionName: sortAscending ? qsTr("ascending") : qsTr("descending")
    readonly property string directionToolTip: sortAscending
        ? qsTr("Sorted ascending (lowest, oldest or A first). Click to sort descending.")
        : qsTr("Sorted descending (highest, newest or Z first). Click to sort ascending.")
    // Whether the direction button's tooltip is open.
    readonly property bool directionToolTipShown: directionToolTipPopup.visible

    signal sortRequested(int field, bool ascending)

    Label {
        id: sortLabel
        Layout.alignment: Qt.AlignVCenter
        objectName: "sortByLabel"
        opacity: 0.8
        text: qsTr("Sort by")
    }

    ComboBox {
        id: sortCombo
        Layout.alignment: Qt.AlignVCenter
        currentIndex: root.sortField
        implicitContentWidthPolicy: ComboBox.WidestText
        model: root.options
        Accessible.name: qsTr("Sort by")
        onActivated: index => root.sortRequested(index, root.sortAscending)
    }

    Button {
        id: sortDirectionButton

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: sortCombo.height
        // Sized to its label, not the style's 80px minimum button width.
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        leftPadding: 6
        rightPadding: 6
        text: root.sortAscending ? qsTr("▲ Asc") : qsTr("▼ Desc")

        Accessible.name: qsTr("Sort direction: %1").arg(root.directionName)
        Accessible.description: root.directionToolTip

        HoverHandler {
            id: directionHover
        }

        ToolTip {
            id: directionToolTipPopup
            objectName: "sortDirectionToolTip"
            // HoverHandler rather than Control.hovered: the latter stays false on platforms
            // that disable hover effects, which would leave the toggle without a tooltip.
            visible: directionHover.hovered
            delay: 400
            text: root.directionToolTip
        }

        // Both labels are laid out and only the current one is shown, so the button
        // is as wide as the wider of the two and flipping never shifts the row.
        contentItem: Item {
            implicitWidth: Math.max(ascendingLabel.implicitWidth, descendingLabel.implicitWidth)
            implicitHeight: Math.max(ascendingLabel.implicitHeight, descendingLabel.implicitHeight)

            Label {
                id: ascendingLabel
                anchors.centerIn: parent
                visible: root.sortAscending
                font: sortDirectionButton.font
                text: qsTr("▲ Asc")
            }
            Label {
                id: descendingLabel
                anchors.centerIn: parent
                visible: !root.sortAscending
                font: sortDirectionButton.font
                text: qsTr("▼ Desc")
            }
        }

        onClicked: root.sortRequested(root.sortField, !root.sortAscending)
    }
}
