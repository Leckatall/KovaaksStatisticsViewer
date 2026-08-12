import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 4

    property var runModel
    property alias title: titleLabel.text
    property string emptyText: "No runs"
    property bool showSort: true
    property int sortField: 0
    property bool sortAscending: false
    signal runSelected(string hash, double startTimeMs)
    signal sortRequested(int field, bool ascending)

    function hasNoRuns() {
        if (!runModel)
            return true
        return runModel.count !== undefined ? runModel.count === 0 : runModel.length === 0
    }

    RowLayout {
        Layout.fillWidth: true

        Label {
            id: titleLabel
            font.bold: true
            // preferredWidth 0 keeps the (arbitrarily long) scenario name out of the
            // layout's width hint; it still takes the row's slack via fillWidth.
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 60
            elide: Text.ElideRight
        }

        Loader {
            // Defer creating the ComboBox/Popup machinery until this view is
            // actually shown; instantiating it while hidden was observed to
            // interfere with unrelated mouse-click delivery elsewhere in the
            // window (e.g. Qt Quick Controls Popup/Overlay setup).
            active: root.showSort && root.visible
            sourceComponent: sortControlsComponent
        }
    }

    Component {
        id: sortControlsComponent

        SortControls {
            comboObjectName: "runSortCombo"
            buttonObjectName: "runSortDirectionButton"
            options: ["Date", "Score", "Accuracy", "Duration"]
            sortField: root.sortField
            sortAscending: root.sortAscending
            onSortRequested: (field, ascending) => {
                root.sortField = field
                root.sortAscending = ascending
                root.sortRequested(field, ascending)
            }
        }
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
    }
}
