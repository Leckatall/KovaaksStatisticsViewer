import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    // Driven from outside (the view model's active scenario); clicking a row only
    // emits scenarioActivated.
    property string currentScenarioHash
    property string emptyText: qsTr("No scenarios yet")
    property string emptyHint: qsTr("Runs you play in Kovaaks are imported from your Kovaaks folder automatically. "
                                     + "If you have played before, check the folder under File › Set Source Directory.")
    property var scenarioModel
    property alias searchText: searchField.text
    property bool showSort: true
    property bool sortAscending: false
    property int sortField: 0

    readonly property int scenarioCount: !scenarioModel ? 0
                                         : (scenarioModel.count !== undefined ? scenarioModel.count : scenarioModel.length)

    // Height that shows every scenario (or the empty state) without spare space;
    // SelectionPanel caps the section at this so a short catalogue leaves the room
    // to the run list.
    readonly property real fittedHeight: headerRow.implicitHeight + searchField.implicitHeight + 2 * spacing
                                         + (scenarioCount === 0 ? emptyState.implicitHeight + 24
                                                                : scenarioList.contentHeight)

    signal scenarioActivated(string hash, string name)

    // Ctrl+F: put the caret in the search field with its text selected, ready to replace.
    function focusSearch() {
        searchField.forceActiveFocus(Qt.ShortcutFocusReason)
        searchField.selectAll()
    }
    signal scenarioSortRequested(int field, bool ascending)
    signal searchEdited(string text)

    spacing: 4

    RowLayout {
        id: headerRow

        Layout.fillWidth: true

        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 60
            elide: Text.ElideRight
            font.bold: true
            objectName: "scenarioListTitle"
            text: qsTr("Scenarios")
        }
        SortControls {
            visible: root.showSort
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

    TextField {
        id: searchField

        Layout.fillWidth: true
        // Without this the style's own implicit width sets the panel's floor.
        Layout.preferredWidth: 120

        objectName: "scenarioSearchField"
        placeholderText: qsTr("Search scenarios…")

        onTextEdited: root.searchEdited(text)
    }

    ListView {
        id: scenarioList

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumHeight: 60
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        currentIndex: -1
        model: root.scenarioModel
        objectName: "scenarioListView"

        ScrollBar.vertical: ListScrollBar {
            objectName: "scenarioListScrollBar"
            overflowing: scenarioList.contentHeight > scenarioList.height + 1
        }

        delegate: ItemDelegate {
            id: delegateRoot

            required property string hash
            required property int index
            required property string name
            required property int runCount

            highlighted: root.currentScenarioHash === hash
            implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
            objectName: "scenarioItem_" + index
            width: scenarioList.width - (scenarioList.ScrollBar.vertical.visible ? scenarioList.ScrollBar.vertical.width : 0)

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

            onClicked: root.scenarioActivated(hash, name)
        }

        // The empty state sits at the top of the list area, where the eye already is.
        ColumnLayout {
            id: emptyState

            objectName: "scenarioListEmptyState"
            parent: scenarioList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 6
            visible: root.scenarioCount === 0

            Label {
                Layout.fillWidth: true
                objectName: "scenarioListEmptyLabel"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: root.searchText !== ""
                      ? qsTr("No scenarios match “%1”.").arg(root.searchText)
                      : root.emptyText
            }
            Label {
                Layout.fillWidth: true
                objectName: "scenarioListEmptyHint"
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.75
                wrapMode: Text.Wrap
                text: root.searchText !== "" ? qsTr("Clear the search to see every scenario.") : root.emptyHint
            }
        }
    }
}
