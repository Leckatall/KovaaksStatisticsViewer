pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
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

    // Dismissal is the view model's call: the window stays until previewOpen goes false.
    onClosing: close => {
        close.accepted = false
        root.captureVm.closePreview()
    }

    function startExport() {
        if (!root.captureVm.beginExport()) return
        saveDialog.currentFile = saveDialog.currentFolder + "/" + root.captureVm.suggestedFileName
        root.savePickerOpen = true
        saveDialog.open()
    }

    // Window-wide so Escape works from the footer buttons, which keep focus after Copy.
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && !root.savePickerOpen
        onActivated: root.captureVm.closePreview()
    }

    FileDialog {
        id: saveDialog
        objectName: "captureSaveDialog"
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
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

            ColumnLayout {
                id: options
                width: optionsScroll.width - optionsScroll.ScrollBar.vertical.implicitWidth
                objectName: "captureOptions"
                spacing: 6

                Label {
                    objectName: "captureOutputSize"
                    text: root.captureVm && root.captureVm.outputWidth > 0
                        ? qsTr("%1 × %2 px").arg(root.captureVm.outputWidth).arg(root.captureVm.outputHeight)
                        : ""
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label { text: qsTr("Resolution") }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Slider {
                            id: resolution
                            objectName: "captureResolution"
                            Layout.fillWidth: true
                            from: 50
                            to: 500
                            stepSize: 10
                            snapMode: Slider.SnapAlways
                            enabled: !root.locked
                            value: root.captureVm ? root.captureVm.percent : 200
                            Accessible.name: qsTr("Resolution")
                            onMoved: root.captureVm.setPercent(Math.round(value))
                        }
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: markMetrics.height

                            FontMetrics {
                                id: markMetrics
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: [50, 100, 200, 300, 400, 500]

                                Label {
                                    required property int modelData
                                    objectName: "captureMark_" + modelData
                                    text: qsTr("%1%").arg(modelData)
                                    font.pixelSize: 11
                                    // The handle's centre travels the track minus one handle width.
                                    x: resolution.leftPadding + resolution.handle.width / 2
                                       + (resolution.availableWidth - resolution.handle.width)
                                       * (modelData - resolution.from) / (resolution.to - resolution.from) - width / 2
                                }
                            }
                        }
                    }
                    Label {
                        objectName: "captureResolutionValue"
                        text: qsTr("%1%").arg(root.captureVm ? root.captureVm.percent : 200)
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
                    sourceComponent: GridLayout {
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 4

                        Label {
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Width and height set the final image size in pixels at the current resolution.")
                        }

                        CheckBox {
                            objectName: "captureKeepAspect"
                            Layout.columnSpan: 3
                            enabled: !root.locked
                            checked: root.captureVm.keepAspect === true
                            text: qsTr("Keep aspect ratio")
                            onToggled: root.captureVm.setKeepAspect(checked)
                        }

                        Label { text: qsTr("Width (px)") }
                        TextField {
                            objectName: "captureWidth"
                            enabled: !root.locked && root.captureVm.hasReference
                            text: root.captureVm.widthText
                            inputMethodHints: Qt.ImhDigitsOnly
                            Accessible.name: qsTr("Width in pixels")
                            Accessible.description: root.captureVm.widthError
                            onEditingFinished: root.captureVm.commitWidth(text)
                        }
                        Label {
                            objectName: "captureWidthError"
                            Layout.fillWidth: true
                            visible: text !== ""
                            wrapMode: Text.WordWrap
                            color: Theme.errorText
                            text: root.captureVm.widthError
                        }

                        Label { text: qsTr("Height (px)") }
                        TextField {
                            objectName: "captureHeight"
                            enabled: !root.locked && root.captureVm.hasReference
                            text: root.captureVm.heightText
                            inputMethodHints: Qt.ImhDigitsOnly
                            Accessible.name: qsTr("Height in pixels")
                            Accessible.description: root.captureVm.heightError
                            onEditingFinished: root.captureVm.commitHeight(text)
                        }
                        Label {
                            objectName: "captureHeightError"
                            Layout.fillWidth: true
                            visible: text !== ""
                            wrapMode: Text.WordWrap
                            color: Theme.errorText
                            text: root.captureVm.heightError
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
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100

        Rectangle {
            objectName: "captureImageFocusRing"
            anchors.fill: parent
            z: 1
            color: "transparent"
            border.width: 2
            border.color: root.palette.highlight
            visible: scroller.activeFocus
        }

        Flickable {
            id: scroller
            objectName: "captureImageScroll"
            anchors.fill: parent
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

    footer: RowLayout {
        spacing: 8

        CaptureStatusStrip {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: 12
            captureVm: root.captureVm
        }
        Button {
            objectName: "captureCloseButton"
            text: qsTr("Close")
            onClicked: root.captureVm.closePreview()
        }
        ChromeIconButton {
            objectName: "captureCopyButton"
            icon.source: "../icons/copy.svg"
            text: qsTr("Copy image to clipboard")
            Accessible.name: text
            ToolTip.text: text
            ToolTip.visible: pointerHovered
            enabled: root.captureVm !== null && root.captureVm.canOutput === true
            onClicked: root.captureVm.copyPreview()
        }
        Button {
            objectName: "captureExportButton"
            Layout.rightMargin: 12
            highlighted: true
            text: qsTr("Export…")
            enabled: root.captureVm !== null && root.captureVm.canOutput === true
            onClicked: root.startExport()
        }
    }
}
