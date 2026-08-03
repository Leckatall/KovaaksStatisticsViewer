import QtQuick.Controls

MenuBar {
    signal setSourceDirRequested()

    Menu {
        title: qsTr("&File")
        Action {
            text: qsTr("&New...")
        }
        Action {
            id: setSoruceDirAction
            text: qsTr("Set Source Directory")
            onTriggered: setSourceDirRequested()
        }

        Action {
            text: qsTr("&Save")
        }
        Action {
            text: qsTr("Save &As...")
        }
        Action {
            text: qsTr("Settings")
        }
        MenuSeparator {
        }
        Action {
            text: qsTr("&Quit")
        }
    }
    Menu {
        title: qsTr("&Help")
        Action {
            text: qsTr("&About")
        }
    }
}
