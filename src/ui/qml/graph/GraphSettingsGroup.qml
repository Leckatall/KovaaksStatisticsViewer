import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One titled group of the Graphs page. A group with no applicable control is hidden whole by its owner.
ColumnLayout {
    id: group

    // dataRange, axes or seriesStyle: the suffix of the header's objectName.
    required property string groupName
    property string title

    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 8

    Label {
        id: header
        objectName: "graphGroup_" + group.groupName
        Layout.fillWidth: true
        text: group.title
        font.bold: true
        Accessible.role: Accessible.Heading
        Accessible.name: group.title
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: header.palette.mid
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: 10
    }
}
