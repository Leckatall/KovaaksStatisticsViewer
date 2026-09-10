import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var trackingVm

    background: Rectangle {
        border.color: root.palette.mid
        color: root.palette.base
        radius: 8
    }

    component Field: RowLayout {
        id: fieldRow
        property string caption
        property string value
        property string valueObjectName
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: fieldRow.caption
            color: root.palette.placeholderText
            Layout.preferredWidth: 120
        }
        Label {
            objectName: fieldRow.valueObjectName
            text: fieldRow.value
            font.bold: true
            Accessible.description: fieldRow.caption + ": " + fieldRow.value
        }
    }

    GridLayout {
        anchors.fill: parent
        columns: 2
        columnSpacing: 16
        rowSpacing: 6

        Field {
            caption: qsTr("Attained rank")
            value: root.trackingVm ? root.trackingVm.attainedRank : ""
            valueObjectName: "summaryAttainedRank"
        }
        Field {
            caption: qsTr("Completed rank")
            value: root.trackingVm ? root.trackingVm.completedRank : ""
            valueObjectName: "summaryCompletedRank"
        }
        Field {
            caption: qsTr("Average rank")
            value: root.trackingVm ? root.trackingVm.averageRank : ""
            valueObjectName: "summaryAverageRank"
        }
        Field {
            caption: qsTr("Total playtime")
            value: root.trackingVm ? root.trackingVm.totalPlaytime : ""
            valueObjectName: "summaryTotalPlaytime"
        }
        Field {
            caption: qsTr("Next tier")
            value: root.trackingVm ? root.trackingVm.nextTier : ""
            valueObjectName: "summaryNextTier"
        }
    }
}
