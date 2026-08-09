import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    property var scenarioModel
    property alias searchText: searchField.text
    property string emptyText: "No scenarios"
    property string currentScenarioHash
    signal searchEdited(string text)
    signal scenarioActivated(string hash, string name)

    TextField {
        id: searchField
        objectName: "scenarioSearchField"
        Layout.fillWidth: true
        placeholderText: "Search scenarios…"
        onTextEdited: root.searchEdited(text)
    }

    ListView {
        id: scenarioList
        objectName: "scenarioListView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 100
        clip: true
        model: root.scenarioModel
        currentIndex: -1

        delegate: ItemDelegate {
            id: delegateRoot
            objectName: "scenarioItem_" + index
            width: ListView.view.width
            implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
            highlighted: root.currentScenarioHash === hash
            required property int index
            required property string name
            required property string hash
            required property int runCount

            contentItem: RowLayout {
                Label { Layout.fillWidth: true; text: delegateRoot.name; elide: Text.ElideRight }
                Label { text: delegateRoot.runCount; color: Material.foreground; opacity: 0.65 }
            }
            onClicked: {
                root.currentScenarioHash = hash
                root.scenarioActivated(hash, name)
            }
        }
    }

    Label {
        objectName: "scenarioListEmptyLabel"
        Layout.alignment: Qt.AlignHCenter
        visible: !root.scenarioModel || root.scenarioModel.count === 0
        text: root.emptyText
        color: Material.foreground
    }
}
