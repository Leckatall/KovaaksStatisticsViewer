import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

ColumnLayout {
    id: libraryPane
    objectName: "libraryPane"
    spacing: 8

    property var manager: null
    // Owned by the caller: the pane reports a new selection through entrySelected and never
    // writes this itself, so the caller's binding survives.
    property string selectedEntryFilename: ""
    property string importStatus: ""
    property string statusMessage: ""
    property bool statusIsError: false

    signal entrySelected(string filename)
    signal newRequested()
    signal importRequested()
    signal openRequested(var entry)
    signal deleteRequested(var entry)
    signal refreshRequested()

    readonly property real toolbarImplicitWidth: libraryToolbar.implicitWidth
    readonly property var libraryEntries: libraryPane.manager ? libraryPane.manager.libraryEntries : []

    function selectedRow() {
        const entries = libraryPane.libraryEntries
        if (!libraryPane.selectedEntryFilename || !entries) return null
        return entries.find(entry => entry.filename === libraryPane.selectedEntryFilename) || null
    }

    function revealEntry(index) {
        libraryList.positionViewAtIndex(index, ListView.Contain)
        libraryList.forceActiveFocus()
    }

    Label {
        text: qsTr("Library")
        font.pixelSize: 18
        font.bold: true
    }

    // One row at every width the pane can take: the buttons size to their text, and
    // the pane never narrows below the row.
    RowLayout {
        id: libraryToolbar
        objectName: "libraryToolbar"
        Layout.fillWidth: true
        spacing: 4

        ToolbarButton {
            objectName: "newBenchmarkButton"
            text: qsTr("New")
            onClicked: libraryPane.newRequested()
        }
        ToolbarButton {
            objectName: "importPlaylistButton"
            text: qsTr("Import...")
            Accessible.name: qsTr("Import playlist")
            ToolTip.text: qsTr("Import a playlist file as a benchmark")
            onClicked: libraryPane.importRequested()
        }
        ToolbarButton {
            objectName: "openBenchmarkButton"
            text: qsTr("Open")
            enabled: libraryPane.selectedRow() !== null && libraryPane.selectedRow().openable
            onClicked: libraryPane.openRequested(libraryPane.selectedRow())
        }
        ToolbarButton {
            objectName: "deleteBenchmarkButton"
            text: qsTr("Delete")
            enabled: libraryPane.selectedRow() !== null && libraryPane.selectedRow().deletable
            onClicked: libraryPane.deleteRequested(libraryPane.selectedRow())
        }
        Item { Layout.fillWidth: true }
        ToolbarButton {
            objectName: "refreshLibraryButton"
            icon.source: "../icons/refresh-cw.svg"
            display: AbstractButton.IconOnly
            enabled: !!libraryPane.manager && !libraryPane.manager.dirty
            Accessible.name: qsTr("Refresh")
            ToolTip.text: qsTr("Refresh the list from the benchmark directory")
            onClicked: libraryPane.refreshRequested()
        }
        ToolbarButton {
            objectName: "openDirectoryButton"
            icon.source: "../icons/folder-open.svg"
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Open directory")
            ToolTip.text: qsTr("Open the benchmark directory in the file manager")
            onClicked:
                Qt.openUrlExternally(encodeURI("file:///" + libraryPane.manager.managedDirectoryPath))
        }
    }

    Label {
        objectName: "importStatusLabel"
        visible: libraryPane.importStatus !== ""
        text: libraryPane.importStatus
        color: libraryPane.importStatus.toLowerCase().indexOf("imported") === 0
            ? libraryPane.palette.windowText : Theme.errorText
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Label {
        objectName: "refreshFailedLabel"
        visible: !!libraryPane.manager && libraryPane.manager.refreshFailed === true
        text: qsTr("Last refresh failed: the directory could not be read.")
        color: Theme.errorText
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Label {
        objectName: "saveStatusLabel"
        visible: libraryPane.statusMessage !== ""
        text: libraryPane.statusMessage
        color: libraryPane.statusIsError ? Theme.errorText : libraryPane.palette.windowText
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    ListView {
        id: libraryList
        objectName: "libraryList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: libraryPane.libraryEntries
        // The selection (selectedEntryFilename) is the single source of truth, so the
        // arrow keys move it directly instead of a separate current index.
        keyNavigationEnabled: false
        activeFocusOnTab: true
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Benchmark library")

        function step(delta) {
            const entries = libraryPane.libraryEntries || []
            if (entries.length === 0) return
            let index = entries.findIndex(entry => entry.filename === libraryPane.selectedEntryFilename)
            index = index < 0 ? (delta > 0 ? 0 : entries.length - 1)
                              : Math.max(0, Math.min(entries.length - 1, index + delta))
            libraryPane.entrySelected(entries[index].filename)
            libraryList.positionViewAtIndex(index, ListView.Contain)
        }
        Keys.onDownPressed: libraryList.step(1)
        Keys.onUpPressed: libraryList.step(-1)
        // Enter opens the selected benchmark, exactly like the Open button.
        Keys.onReturnPressed: libraryPane.openRequested(libraryPane.selectedRow())
        Keys.onEnterPressed: libraryPane.openRequested(libraryPane.selectedRow())

        delegate: ItemDelegate {
            id: libraryEntry
            required property var modelData
            required property int index
            width: libraryList.width
            objectName: "libraryEntry_" + index
            // The list owns the keyboard focus; the rows only select.
            focusPolicy: Qt.NoFocus
            highlighted: libraryPane.selectedEntryFilename === libraryEntry.modelData.filename
            onClicked: {
                libraryPane.entrySelected(libraryEntry.modelData.filename)
                libraryList.forceActiveFocus()
            }
            // Double-click opens through the same guard as the Open button.
            onDoubleClicked: {
                libraryPane.entrySelected(libraryEntry.modelData.filename)
                libraryList.forceActiveFocus()
                libraryPane.openRequested(libraryEntry.modelData)
            }
            contentItem: RowLayout {
                spacing: 8
                Label {
                    text: libraryEntry.modelData.name
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.classificationColor(libraryEntry.modelData.classification)
                }
                Label {
                    text: libraryEntry.modelData.classification
                    color: libraryEntry.palette.placeholderText
                }
            }
        }
    }
}
