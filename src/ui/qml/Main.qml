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


    menuBar: AppMenuBar {
        onSetSourceDirRequested: folderDialog.open()
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
        }
        ControlPanel {
            Layout.row: 1; Layout.column: 0
            sessionVm: root.sessionVm
            graphVm: root.graphVm
        }
    }
}
