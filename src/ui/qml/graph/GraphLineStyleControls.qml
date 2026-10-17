import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "GraphCatalog.js" as GraphCatalog

// A graph's line style and point-marker choice, as the gear popup and the Graphs page both offer it. Each
// control reports the choice and then follows `connection` / `markers` again, which the owner may not
// have changed.
ColumnLayout {
    id: root

    // smooth, straight or pointsOnly.
    property string connection
    property bool markers

    readonly property alias connectionCombo: connectionCombo
    readonly property bool pointsOnly: connection === "pointsOnly"

    signal connectionChosen(string connection)
    signal markersChosen(bool markers)

    function connectionIndex() {
        return Math.max(0, GraphCatalog.connectionModes.findIndex(e => e.value === root.connection))
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label { text: qsTr("Line style") }
        ComboBox {
            id: connectionCombo
            objectName: "graphField_seriesStyle_connection"
            Layout.fillWidth: true
            model: GraphCatalog.connectionModes
            textRole: "text"
            currentIndex: root.connectionIndex()
            Accessible.name: qsTr("Line style")
            onActivated: index => {
                root.connectionChosen(GraphCatalog.connectionModes[index].value)
                connectionCombo.currentIndex = Qt.binding(() => root.connectionIndex())
            }
        }
    }

    CheckBox {
        id: markerBox
        objectName: "graphField_seriesStyle_markers"
        Layout.fillWidth: true
        text: qsTr("Show point markers")
        checked: root.pointsOnly || root.markers
        enabled: !root.pointsOnly
        Accessible.name: text
        Accessible.description: root.pointsOnly ? qsTr("Always shown while Points only is selected") : ""
        onToggled: {
            root.markersChosen(markerBox.checked)
            markerBox.checked = Qt.binding(() => root.pointsOnly || root.markers)
        }
    }
}
