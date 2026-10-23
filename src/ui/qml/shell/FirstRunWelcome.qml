import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// What the Scenarios workspace shows while no usable Kovaaks folder is configured and
// nothing has been imported: a welcome whose one primary action is choosing the
// folder, instead of a screen of empty graphs.
Item {
    id: root

    signal chooseFolderRequested()

    Rectangle {
        id: card

        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        height: content.implicitHeight + 64
        color: root.palette.base
        border.color: root.palette.mid
        radius: 10

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: 32
            spacing: 14

            Label {
                // preferredWidth 0: wrap to the card instead of widening it.
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                objectName: "firstRunWelcomeTitle"
                font.pixelSize: 24
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Welcome to Kovaaks Stats Viewer")
            }
            Label {
                // preferredWidth 0: wrap to the card instead of widening it.
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Choose your Kovaaks folder to import your runs. Your scenarios, run graphs, "
                           + "run history and playtime will appear here.")
            }
            Button {
                id: chooseButton

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                objectName: "welcomeChooseKovaaksFolderButton"
                highlighted: true
                font.pixelSize: 15
                font.bold: true
                leftPadding: 28
                rightPadding: 28
                topPadding: 10
                bottomPadding: 10
                text: qsTr("Choose Kovaaks Folder…")
                Accessible.name: qsTr("Choose Kovaaks Folder")

                // A filled accent button: the one primary action on the screen.
                background: Rectangle {
                    implicitHeight: 40
                    radius: 6
                    color: chooseButton.down ? Qt.darker(chooseButton.palette.highlight, 1.2)
                         : chooseButton.hovered ? Qt.lighter(chooseButton.palette.highlight, 1.15)
                         : chooseButton.palette.highlight
                    border.width: chooseButton.visualFocus ? 2 : 0
                    border.color: chooseButton.palette.accent
                }
                contentItem: Label {
                    text: chooseButton.text
                    font: chooseButton.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.chooseFolderRequested()
            }
            Label {
                // preferredWidth 0: wrap to the card instead of widening it.
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.topMargin: 6
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.7
                wrapMode: Text.Wrap
                text: qsTr("It is the FPSAimTrainer folder in your Steam library, usually\n"
                           + "C:\\Program Files (x86)\\Steam\\steamapps\\common\\FPSAimTrainer")
            }
        }
    }
}
