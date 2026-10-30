pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "GraphCatalog.js" as GraphCatalog

// Every graph's immediate show/hide, grouped by workspace.
Menu {
    id: root

    property var visualSettings: null

    component GroupHeading: Label {
        leftPadding: 8
        topPadding: 6
        bottomPadding: 6
        color: Qt.alpha(palette.text, 0.78)
        Accessible.role: Accessible.Heading
    }

    component GraphItem: MenuItem {
        required property var modelData
        objectName: "graphVisibility_" + modelData.key
        text: modelData.name
        checkable: true
        checked: root.visualSettings ? root.visualSettings.isGraphVisible(modelData.key) : true
        onTriggered: if (root.visualSettings) root.visualSettings.setGraphVisible(modelData.key, checked)
    }

    GroupHeading { text: qsTr("Scenarios") }
    Repeater {
        model: GraphCatalog.graphs.filter(graph => graph.workspace === "Scenarios")
        GraphItem {}
    }
    MenuSeparator {}
    GroupHeading { text: qsTr("Benchmarks") }
    Repeater {
        model: GraphCatalog.graphs.filter(graph => graph.workspace === "Benchmarks")
        GraphItem {}
    }
}
