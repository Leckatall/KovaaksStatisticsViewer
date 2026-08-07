import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

ApplicationWindow {
    id: root
    width: 1200
    height: 800

    visible: true
    title: "Kovaaks Stats Viewer"
    Material.theme: Material.Dark
    Material.accent: Material.Cyan
    Material.primary: Material.BlueGrey

    Settings {
        category: "window"
        property alias width: root.width
        property alias height: root.height
    }

    // Map of column name -> visibility, keyed dynamically off graphVm.columnName().
    //
    // Named distinctly from the `columnVisibility` property that
    // DashboardGraph/ControlPanel/SettingsDialog each declare: binding e.g.
    // `columnVisibility: columnVisibility` inside one of those components
    // resolves the RHS against the component's own property of the same
    // name first, silently producing a circular/undefined binding instead
    // of referencing this Settings object.
    Settings {
        id: columnVisibilitySettings
        category: "graphColumns"
        property bool score: true
        property bool accuracy: true
        property bool shots: true
        property bool kills: true
        property bool dmg: true
    }

    required property var graphVm
    required property var sessionVm
    required property var settingsVm

    Rectangle {
        anchors.fill: parent
        color: "#121212"
    }

    FolderDialog {
        id: folderDialog
        currentFolder: root.settingsVm.kovaaksDir
        onAccepted: root.settingsVm.setKovaaksDir(folderDialog.selectedFolder)
    }

    Loader {
        id: settingsDialogLoader
        active: false
        sourceComponent: SettingsDialog {
            settingsVm: root.settingsVm
            graphVm: root.graphVm
            columnVisibility: columnVisibilitySettings
        }
        onLoaded: item.open()
    }

    menuBar: AppMenuBar {
        onSetSourceDirRequested: folderDialog.open()
        onSettingsRequested: {
            if (settingsDialogLoader.active) settingsDialogLoader.item.open()
            else settingsDialogLoader.active = true
        }
    }

    header: KovaaksDirToolBar {
        kovaaksDir: root.settingsVm.kovaaksDir
    }
    GridLayout {
        anchors.fill: parent
        anchors.margins: 5
        Label {
            Layout.row: 0; Layout.column: 0
            Layout.columnSpan: 2
            text: "Dashboard"
            font.pixelSize: 24
            font.bold: true
            color: "white"
        }

        DashboardGraph {
            Layout.row: 1; Layout.column: 1
            graphVm: root.graphVm
            columnVisibility: columnVisibilitySettings
        }
        ControlPanel {
            Layout.row: 1; Layout.column: 0
            sessionVm: root.sessionVm
            graphVm: root.graphVm
            columnVisibility: columnVisibilitySettings
        }
    }
}
