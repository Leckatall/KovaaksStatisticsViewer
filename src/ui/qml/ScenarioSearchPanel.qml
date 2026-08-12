import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    property var scenarioModel
    property alias searchText: searchField.text
    property string emptyText: "No scenarios"
    property string currentScenarioHash
    property bool showSort: true
    property int sortField: 0
    property bool sortAscending: false
    signal searchEdited(string text)
    signal scenarioActivated(string hash, string name)
    signal scenarioSortRequested(int field, bool ascending)

    TextField {
        id: searchField
        objectName: "scenarioSearchField"
        Layout.fillWidth: true
        placeholderText: "Search scenarios…"
        onTextEdited: root.searchEdited(text)
    }

    Loader {
        // Deferred for the same reason as RunListView's sort controls: instantiating
        // the ComboBox/Popup machinery while hidden interferes with unrelated mouse
        // clicks elsewhere in the window.
        active: root.showSort && root.visible
        sourceComponent: sortControlsComponent
    }

    Component {
        id: sortControlsComponent

        RowLayout {
            spacing: 4

            Label {
                text: "Sort:"
            }

            ComboBox {
                id: sortCombo
                objectName: "scenarioSortCombo"
                model: ["Runs", "Last Played", "Name"]
                currentIndex: root.sortField
                onActivated: (index) => {
                    root.sortField = index
                    root.scenarioSortRequested(root.sortField, root.sortAscending)
                }
            }

            ToolButton {
                id: sortDirectionButton
                objectName: "scenarioSortDirectionButton"
                text: root.sortAscending ? "▲" : "▼"
                onClicked: {
                    root.sortAscending = !root.sortAscending
                    root.scenarioSortRequested(root.sortField, root.sortAscending)
                }
            }
        }
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
                Label { text: delegateRoot.runCount; opacity: 0.65 }
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
    }
}
