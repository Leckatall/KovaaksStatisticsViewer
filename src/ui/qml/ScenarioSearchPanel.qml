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
        id: searchRow

        Layout.fillWidth: true

        TextField {
            id: searchField

            Layout.fillWidth: true
            // Without this the style's own implicit width sets the panel's floor.
            Layout.preferredWidth: 120

            objectName: "scenarioSearchField"
            placeholderText: "Search scenarios…"

            onTextEdited: root.searchEdited(text)
        }
        SortControls {
            comboObjectName: "scenarioSortCombo"
            buttonObjectName: "scenarioSortDirectionButton"
            options: ["Last Played", "Runs", "Name"]
            sortField: root.sortField
            sortAscending: root.sortAscending
            onSortRequested: (field, ascending) => {
                root.sortField = field
                root.sortAscending = ascending
                root.scenarioSortRequested(field, ascending)
            }
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
