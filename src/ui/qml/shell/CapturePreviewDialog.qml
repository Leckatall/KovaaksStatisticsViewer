pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

ApplicationWindow {
    id: root
    objectName: "capturePreviewDialog"

    property var captureVm: null

    readonly property string previewState: root.captureVm ? root.captureVm.previewState : "rendering"
    readonly property bool unavailable: root.previewState === "invalid" || root.previewState === "failed"
    readonly property var image: root.captureVm ? root.captureVm.previewImage : null
    readonly property bool locked: root.captureVm === null || root.captureVm.optionsLocked === true
    property bool advancedExpanded: false
    readonly property bool hasImage: root.captureVm !== null && root.captureVm.previewImageWidth > 0

    title: qsTr("Export image: %1").arg(root.captureVm ? root.captureVm.previewTargetName : "")
    flags: Qt.Dialog
    modality: Qt.WindowModal
    palette: Theme.palette
    width: 900
    height: 640
    minimumWidth: 560
    minimumHeight: 360
    visible: root.captureVm !== null && root.captureVm.previewOpen === true
    onVisibleChanged: if (!visible) root.advancedExpanded = false

    // Set just before the picker opens and cleared when it returns, so it does not depend on how
    // FileDialog.visible behaves under every platform.
    property bool savePickerOpen: false

    // Dismissal is the view model's call: the window stays until previewOpen goes false. A hidden
    // preview must still accept: quitting closes every window the app has shown, and one refusal
    // cancels the quit, leaving the process running with no window.
    onClosing: close => {
        if (root.captureVm === null || root.captureVm.previewOpen !== true) return
        close.accepted = false
        root.captureVm.closePreview()
    }

    function startExport() {
        if (!root.captureVm.beginExport()) return
        root.savePickerOpen = true
        saveDialog.openFor(root.captureVm.suggestedFileName)
    }

    // Window-wide so Escape works from the footer buttons, which keep focus after Copy.
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !root.savePickerOpen
        onActivated: root.captureVm.closePreview()
    }

    CaptureFileDialog {
        id: saveDialog
        objectName: "captureSaveDialog"
        onAccepted: {
            root.savePickerOpen = false
            root.captureVm.confirmExport(saveDialog.selectedFile)
        }
        onRejected: {
            root.savePickerOpen = false
            root.captureVm.cancelExport()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // The options stay editable in every state so an unrenderable choice can be corrected; only
        // optionsLocked pins them, control by control, so the advanced section can still be expanded.
        Flickable {
            id: optionsScroll
            objectName: "captureOptionsScroll"
            Layout.fillWidth: true
            Layout.minimumHeight: 60
            Layout.preferredHeight: Math.min(options.implicitHeight, Math.max(60, root.height * 0.3))
            clip: true
            contentWidth: width
            contentHeight: options.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            // Flickable does not follow keyboard focus, so Tab can land on a control clipped out of view.
            Connections {
                target: root
                function onActiveFocusItemChanged() {
                    const item = root.activeFocusItem
                    if (!item || !options.contains(options.mapFromItem(item, 0, 0))) return
                    const top = item.mapToItem(optionsScroll.contentItem, 0, 0).y
                    const bottom = top + item.height
                    if (top < optionsScroll.contentY)
                        optionsScroll.contentY = Math.max(0, top)
                    else if (bottom > optionsScroll.contentY + optionsScroll.height)
                        optionsScroll.contentY = Math.min(bottom - optionsScroll.height,
                                                          optionsScroll.contentHeight - optionsScroll.height)
                }
            }

            ColumnLayout {
                id: options
                width: optionsScroll.width - optionsScroll.ScrollBar.vertical.implicitWidth
                objectName: "captureOptions"
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label { text: qsTr("Render resolution") }
                    Label {
                        objectName: "captureResolutionValue"
                        font.bold: true
                        color: root.palette.accent
                        text: qsTr("%1%").arg(root.captureVm ? root.captureVm.percent : 200)
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        objectName: "captureOutputSize"
                        color: root.palette.placeholderText
                        text: root.captureVm && root.captureVm.outputWidth > 0
                            ? qsTr("%1 × %2 px").arg(root.captureVm.outputWidth).arg(root.captureVm.outputHeight)
                            : ""
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Slider {
                        id: resolution
                        objectName: "captureResolution"
                        Layout.fillWidth: true
                        implicitHeight: 24
                        leftPadding: 8
                        rightPadding: 8
                        from: 50
                        to: 500
                        stepSize: 10
                        snapMode: Slider.SnapAlways
                        enabled: !root.locked
                        value: root.captureVm ? root.captureVm.percent : 200
                        Accessible.name: qsTr("Resolution")
                        onMoved: root.captureVm.setPercent(Math.round(value))

                        // The track runs between the handle's extreme centres, so the fill ends under it.
                        background: Rectangle {
                            x: resolution.leftPadding + resolution.handle.width / 2
                            y: (resolution.height - height) / 2
                            width: resolution.availableWidth - resolution.handle.width
                            height: 4
                            radius: 2
                            color: resolution.palette.midlight

                            Rectangle {
                                width: resolution.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: resolution.enabled ? resolution.palette.highlight : resolution.palette.light
                            }
                        }
                        handle: Rectangle {
                            x: resolution.leftPadding + resolution.visualPosition * (resolution.availableWidth - width)
                            y: (resolution.height - height) / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: resolution.palette.windowText
                            border.width: resolution.visualFocus ? 3 : 2
                            border.color: !resolution.enabled ? resolution.palette.light
                                        : resolution.visualFocus ? resolution.palette.accent
                                        : resolution.palette.highlight
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 7 + markMetrics.height

                        FontMetrics {
                            id: markMetrics
                            font.pixelSize: 11
                        }
                        Repeater {
                            model: [50, 100, 200, 300, 400, 500]

                            Item {
                                id: mark
                                required property int modelData
                                x: resolution.leftPadding + resolution.handle.width / 2
                                   + (resolution.availableWidth - resolution.handle.width)
                                   * (modelData - resolution.from) / (resolution.to - resolution.from)

                                Rectangle {
                                    x: -0.5
                                    width: 1
                                    height: 5
                                    color: resolution.palette.light
                                }
                                Label {
                                    objectName: "captureMark_" + mark.modelData
                                    x: -width / 2
                                    y: 7
                                    text: qsTr("%1%").arg(mark.modelData)
                                    font.pixelSize: 11
                                    color: root.captureVm && root.captureVm.percent === mark.modelData
                                        ? root.palette.windowText : root.palette.placeholderText
                                }
                            }
                        }
                    }
                }

                CheckBox {
                    objectName: "captureSummary"
                    visible: root.captureVm !== null && root.captureVm.isGraph !== true
                    enabled: !root.locked && root.captureVm !== null && root.captureVm.summaryAvailable === true
                    checked: root.captureVm !== null && root.captureVm.includeSummary === true
                    text: qsTr("Include summary")
                    onToggled: root.captureVm.setIncludeSummary(checked)
                }

                Loader {
                    active: root.captureVm !== null && root.captureVm.isGraph === true
                    visible: active
                    sourceComponent: Button {
                        objectName: "captureAdvanced"
                        flat: true
                        checkable: true
                        checked: root.advancedExpanded
                        text: qsTr("Advanced options")
                        icon.source: checked ? "../icons/chevron-down.svg" : "../icons/chevron-right.svg"
                        onToggled: root.advancedExpanded = checked
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    active: root.advancedExpanded && root.captureVm !== null && root.captureVm.isGraph === true
                    visible: active
                    sourceComponent: ColumnLayout {
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Width and height set the final image size in pixels at the current resolution.")
                        }

                        RowLayout {
                            spacing: 8

                            Label { text: qsTr("Width") }
                            TextField {
                                objectName: "captureWidth"
                                Layout.preferredWidth: 96
                                enabled: !root.locked && root.captureVm.hasReference
                                text: root.captureVm.widthText
                                inputMethodHints: Qt.ImhDigitsOnly
                                Accessible.name: qsTr("Width in pixels")
                                Accessible.description: root.captureVm.widthError
                                onEditingFinished: root.captureVm.commitWidth(text)
                            }
                            Label {
                                text: qsTr("×")
                                color: root.palette.placeholderText
                                Accessible.ignored: true
                            }
                            Label { text: qsTr("Height") }
                            TextField {
                                objectName: "captureHeight"
                                Layout.preferredWidth: 96
                                enabled: !root.locked && root.captureVm.hasReference
                                text: root.captureVm.heightText
                                inputMethodHints: Qt.ImhDigitsOnly
                                Accessible.name: qsTr("Height in pixels")
                                Accessible.description: root.captureVm.heightError
                                onEditingFinished: root.captureVm.commitHeight(text)
                            }
                            Label {
                                text: qsTr("px")
                                color: root.palette.placeholderText
                                Accessible.ignored: true
                            }
                        }

                        // Always visible, so an empty error still reserves its line and Keep aspect ratio
                        // does not jump as errors come and go.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                objectName: "captureWidthError"
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                wrapMode: Text.WordWrap
                                color: Theme.errorText
                                text: root.captureVm.widthError
                            }
                            Label {
                                objectName: "captureHeightError"
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                wrapMode: Text.WordWrap
                                color: Theme.errorText
                                text: root.captureVm.heightError
                            }
                        }

                        CheckBox {
                            objectName: "captureKeepAspect"
                            enabled: !root.locked
                            checked: root.captureVm.keepAspect === true
                            text: qsTr("Keep aspect ratio")
                            onToggled: root.captureVm.setKeepAspect(checked)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.unavailable

            Image {
                objectName: "captureErrorIcon"
                source: "../icons/circle-alert.svg"
                sourceSize.width: 16
                sourceSize.height: 16
                Accessible.ignored: true
            }
            Label {
                objectName: "captureError"
                Accessible.role: Accessible.AlertMessage
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.captureVm ? root.captureVm.previewError : ""
            }
            Button {
                objectName: "captureRetry"
                visible: root.previewState === "failed"
                text: qsTr("Retry")
                onClicked: root.captureVm.retryRender()
            }
        }

        // Fits the width and scrolls vertically so the whole of a tall table is reachable by keyboard.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100 + 2 * scroller.anchors.margins
            radius: 6
            color: Qt.darker(root.palette.window, 1.3)
            border.color: root.palette.button

            Rectangle {
                objectName: "captureImageFocusRing"
                anchors.fill: parent
                z: 1
                radius: parent.radius
                color: "transparent"
                border.width: 2
                border.color: root.palette.highlight
                visible: scroller.activeFocus
            }

            Flickable {
                id: scroller
                objectName: "captureImageScroll"
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                focus: true
                activeFocusOnTab: true
                contentWidth: width
                contentHeight: preview.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                function scrollBy(delta) {
                    contentY = Math.max(0, Math.min(contentHeight - height, contentY + delta))
                }
                Keys.onPressed: event => {
                    const step = 40
                    switch (event.key) {
                    case Qt.Key_Down: scrollBy(step); break
                    case Qt.Key_Up: scrollBy(-step); break
                    case Qt.Key_PageDown: scrollBy(height * 0.9); break
                    case Qt.Key_PageUp: scrollBy(-height * 0.9); break
                    default: return
                    }
                    event.accepted = true
                }

                CapturePreviewItem {
                    id: preview
                    objectName: "captureImage"
                    width: scroller.width
                    height: root.hasImage ? Math.ceil(width * root.captureVm.previewImageHeight / root.captureVm.previewImageWidth) : 0
                    visible: root.hasImage && !root.unavailable
                }
                // Kept off the item while there is none, because a null variant cannot be assigned to a QImage.
                Binding {
                    target: preview
                    property: "image"
                    value: root.image
                    when: root.hasImage
                }
            }
        }
    }

    footer: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            implicitHeight: 1
            color: root.palette.button
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 12

            CaptureActionButton {
                objectName: "captureCloseButton"
                Layout.preferredWidth: Math.max(76, implicitWidth)
                text: qsTr("Close")
                onClicked: root.captureVm.closePreview()
            }
            // Wraps the strip because it hides when idle, and a hidden layout item would stop pushing
            // Copy and Export to the right.
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: status.implicitHeight

                CaptureStatusStrip {
                    id: status
                    width: parent.width
                    anchors.verticalCenter: parent.verticalCenter
                    captureVm: root.captureVm
                }
            }
            CaptureActionButton {
                objectName: "captureCopyButton"
                Layout.preferredWidth: 34
                leftPadding: 0
                rightPadding: 0
                display: AbstractButton.IconOnly
                icon.source: "../icons/copy.svg"
                text: qsTr("Copy image to clipboard")
                Accessible.name: text
                ToolTip.text: text
                enabled: root.captureVm !== null && root.captureVm.canOutput === true
                onClicked: root.captureVm.copyPreview()
            }
            CaptureActionButton {
                objectName: "captureExportButton"
                Layout.preferredWidth: Math.max(122, implicitWidth)
                highlighted: true
                text: qsTr("Export…")
                enabled: root.captureVm !== null && root.captureVm.canOutput === true
                onClicked: root.startExport()
            }
        }
    }
}
