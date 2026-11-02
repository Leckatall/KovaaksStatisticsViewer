import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var captureVm: null

    readonly property string kind: root.captureVm ? root.captureVm.feedbackKind : "none"
    readonly property string message: root.captureVm ? root.captureVm.feedbackText : ""

    visible: root.kind !== "none"
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    onMessageChanged: {
        if (root.message !== "")
            root.Accessible.announce(root.message,
                                     root.kind === "failure" ? Accessible.Assertive : Accessible.Polite)
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: 6

        Image {
            objectName: "captureStatusIcon"
            sourceSize.width: 14
            sourceSize.height: 14
            source: root.kind === "failure" ? "../icons/circle-alert.svg"
                : root.kind === "success" ? "../icons/check.svg"
                : "../icons/info.svg"
            Accessible.ignored: true
        }

        Label {
            objectName: "captureStatusText"
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: 12
            text: root.message
            Accessible.role: root.kind === "failure" ? Accessible.AlertMessage : Accessible.StaticText
        }

        ToolButton {
            objectName: "captureStatusDismiss"
            visible: root.kind === "failure" || root.kind === "notice"
            implicitHeight: 22
            implicitWidth: 22
            padding: 0
            icon.source: "../icons/x.svg"
            icon.width: 12
            icon.height: 12
            Accessible.name: qsTr("Dismiss capture message")
            onClicked: root.captureVm.dismissFeedback()
        }
    }
}
