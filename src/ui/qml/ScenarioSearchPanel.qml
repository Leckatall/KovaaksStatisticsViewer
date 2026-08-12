import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string currentScenarioHash
    property string emptyText: "No scenarios"
    property var scenarioModel
    property alias searchText: searchField.text
    property bool showSort: true
    property bool sortAscending: false
    property int sortField: 0

    signal scenarioActivated(string hash, string name)
    signal scenarioSortRequested(int field, bool ascending)
    signal searchEdited(string text)

    spacing: 6

    RowLayout {
        TextField {
            id: searchField

            Layout.fillWidth: true

            objectName: "scenarioSearchField"
            placeholderText: "Search scenarios…"

            onTextEdited: root.searchEdited(text)
        }
        GridLayout {
            id: searchSortControls
            rowSpacing: 0
            Label {
                Layout.columnSpan: 2
                Layout.row: 0
                Layout.alignment: Qt.AlignCenter
                text: "Sort By:"
            }
            ComboBox {
                id: sortCombo

                Layout.column: 0
                Layout.row: 1
                currentIndex: root.sortField
                model: ["Last Played", "Runs", "Name"]
                objectName: "scenarioSortCombo"

                onActivated: index => {
                    root.sortField = index;
                    root.scenarioSortRequested(root.sortField, root.sortAscending);
                }
            }
            ToolButton {
                id: sortDirectionButton

                Layout.column: 1
                Layout.row: 1
                objectName: "scenarioSortDirectionButton"
                text: root.sortAscending ? "▲" : "▼"

                onClicked: {
                    root.sortAscending = !root.sortAscending;
                    root.scenarioSortRequested(root.sortField, root.sortAscending);
                }
            }
        }
    }
    Component {
        id: sortControlsComponent

        RowLayout {
            spacing: 4
        }
    }
    ListView {
        id: scenarioList

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumHeight: 100
        clip: true
        currentIndex: -1
        model: root.scenarioModel
        objectName: "scenarioListView"

        delegate: ItemDelegate {
            id: delegateRoot

            required property string hash
            required property int index
            required property string name
            required property int runCount

            highlighted: root.currentScenarioHash === hash
            implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
            objectName: "scenarioItem_" + index
            width: ListView.view.width

            contentItem: RowLayout {
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: delegateRoot.name
                }
                Label {
                    opacity: 0.65
                    text: delegateRoot.runCount
                }
            }

            onClicked: {
                root.currentScenarioHash = hash;
                root.scenarioActivated(hash, name);
            }
        }
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        objectName: "scenarioListEmptyLabel"
        text: root.emptyText
        visible: !root.scenarioModel || root.scenarioModel.count === 0
    }
}
