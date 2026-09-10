import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var blockers: []

    spacing: 4

    Label {
        Layout.fillWidth: true
        visible: root.blockers.length === 0
        text: qsTr("No blockers for the next tier.")
        color: root.palette.placeholderText
    }

    Repeater {
        model: root.blockers

        delegate: Label {
            required property var modelData
            required property int index

            objectName: "blocker_" + index
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: modelData.kind === "group"
                  ? qsTr("%1: %2 (%3)").arg(modelData.groupName).arg(modelData.currentText).arg(modelData.requiredText)
                  : qsTr("%1 (%2): %3 / %4").arg(modelData.scenarioName).arg(modelData.groupName)
                        .arg(modelData.currentText).arg(modelData.requiredText)
            Accessible.name: text
        }
    }
}
