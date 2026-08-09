import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 4

    property var runModel
    property alias title: titleLabel.text
    property string emptyText: "No runs"
    signal runSelected(string hash, double startTimeMs)

    function hasNoRuns() {
        if (!runModel)
            return true
        return runModel.count !== undefined ? runModel.count === 0 : runModel.length === 0
    }

    Label {
        id: titleLabel
        font.bold: true
        color: Material.foreground
    }

    ListView {
        id: runList
        objectName: "runListView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        clip: true
        spacing: 6
        model: root.runModel

        delegate: ScenarioRunItem {
            onRunSelected: (hash, startTimeMs) => root.runSelected(hash, startTimeMs)
        }
    }

    Label {
        objectName: "runListEmptyLabel"
        Layout.alignment: Qt.AlignHCenter
        visible: root.hasNoRuns()
        text: root.emptyText
        color: Material.foreground
    }
}
