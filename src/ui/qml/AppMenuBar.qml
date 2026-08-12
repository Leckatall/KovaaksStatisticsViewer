import QtQuick.Controls

MenuBar {
    signal setSourceDirRequested()
    signal settingsRequested()
    signal loadPerformanceFileRequested()

    Menu {
        title: qsTr("&File")
        Action {
            text: qsTr("&New...")
        }
        Action {
            id: setSoruceDirAction
            text: qsTr("Set Source &Directory")
            onTriggered: setSourceDirRequested()
        }
        Action {
            id: loadPerformanceFileAction
            text: qsTr("&Load Performance File...")
            onTriggered: loadPerformanceFileRequested()
        }

        // Action {
        //     text: qsTr("&Save")
        // }
        // Action {
        //     text: qsTr("Save &As...")
        // }
        Action {
            id: settingsAction
            text: qsTr("&Settings")
            onTriggered: settingsRequested()
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
