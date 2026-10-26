pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer
import "axisNames.js" as AxisNames

ScrollView {
    id: root

    required property var settingsVm
    readonly property var overview: settingsVm.axisOverview
    readonly property var sharedAxes: overview && overview.sharedAxes ? overview.sharedAxes : []
    readonly property var independentLines: overview && overview.independentLines ? overview.independentLines : []
    // The delete confirmation lives on the panel, keyed by axis id: confirming removes the axis's row,
    // which would destroy a dialog that row owned.
    property string deleteTargetId: ""
    readonly property var deleteTarget: sharedAxes.find(axis => axis.id === deleteTargetId) || null
    property bool deleteConfirmed: false
    property string createdAxisId: ""
    readonly property alias yAxisGroup: yAxisSelection
    readonly property alias newAxisDialog: axisCreationDialog
    readonly property alias deleteDialog: deleteAxisDialog

    objectName: "axesPanel"
    contentWidth: availableWidth

    Component.onCompleted: syncRows()
    onOverviewChanged: syncRows()
    onDeleteTargetChanged: if (!deleteTarget) deleteAxisDialog.close()

    // ---- Rows ----------------------------------------------------------------------------------------

    // Every VM edit republishes axisOverview whole. Rows are patched in place by id instead of being
    // rebuilt, so a control the user is pressing, or has just tabbed to, survives the edit. This reads
    // overview itself because, inside its change handler, sharedAxes may not have re-evaluated yet.
    function syncRows() {
        const shared = overview && overview.sharedAxes ? overview.sharedAxes : []
        const independent = overview && overview.independentLines ? overview.independentLines : []
        patchModel(axisRows, "axisId", shared.map(axis => ({
            axisId: axis.id,
            key: axis.key,
            name: axis.name,
            nameText: axis.nameText,
            nameIssue: axis.nameIssue || "",
            lineCount: axis.lineNames.length,
            lineNamesText: axis.lineNames.join(", "),
            hasEnabledLine: axis.hasEnabledLine,
            shown: axis.shown
        })))
        patchModel(independentRows, "lineId", independent.map(line => ({
            lineId: line.id,
            key: line.key,
            name: line.name,
            lineEnabled: line.enabled,
            shown: line.shown
        })))
    }

    function patchModel(model, idRole, rows) {
        for (let i = 0; i < rows.length; ++i) {
            let at = -1
            for (let j = i; j < model.count && at < 0; ++j)
                if (model.get(j)[idRole] === rows[i][idRole])
                    at = j
            if (at < 0) {
                model.insert(i, rows[i])
                continue
            }
            if (at !== i)
                model.move(at, i, 1)
            const current = model.get(i)
            for (const role in rows[i])
                if (current[role] !== rows[i][role])
                    model.setProperty(i, role, rows[i][role])
        }
        if (model.count > rows.length)
            model.remove(rows.length, model.count - rows.length)
    }

    function axisRowFor(id) {
        for (let i = 0; i < axisRepeater.count; ++i) {
            const row = axisRepeater.itemAt(i) as AxisRow
            if (row && row.axisId === id)
                return row
        }
        return null
    }

    // ---- Actions -------------------------------------------------------------------------------------

    function chooseYAxis(key, shown) {
        if (!shown)
            settingsVm.setRunAxisLabel(key)
    }

    function requestDelete(id, lineCount) {
        if (lineCount === 0) {
            settingsVm.deleteAxis(id)
            newAxisButton.forceActiveFocus(Qt.TabFocusReason)
            return
        }
        deleteTargetId = id
        deleteAxisDialog.open()
    }

    function confirmDelete() {
        const id = deleteTargetId
        deleteConfirmed = true
        deleteAxisDialog.close()
        settingsVm.deleteAxis(id)
    }

    function focusAxisName(id) {
        const row = axisRowFor(id)
        if (row)
            row.nameField.forceActiveFocus(Qt.OtherFocusReason)
    }

    function ensureVisible(item) {
        const flick = root.contentItem as Flickable
        const top = item.mapToItem(flick.contentItem, 0, 0).y
        const bottom = top + item.height
        const margin = 8
        if (top - margin < flick.contentY)
            flick.contentY = Math.max(0, top - margin)
        else if (bottom + margin > flick.contentY + flick.height)
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, bottom + margin - flick.height))
    }

    function isWithin(item, ancestor) {
        for (let current = item; current; current = current.parent)
            if (current === ancestor)
                return true
        return false
    }

    Window.onActiveFocusItemChanged: {
        const item = root.Window.activeFocusItem
        if (item && isWithin(item, content))
            ensureVisible(item)
    }

    // ---- Content -------------------------------------------------------------------------------------

    ButtonGroup {
        id: yAxisSelection
        exclusive: true
    }

    // Checked state follows the VM's `shown`, never a click, so revert and Cancel restore it; the click
    // only stages the key and the patched row carries the new selection.
    component YAxisButton: ActionButton {
        required property string rowKey
        required property string rowName
        required property bool shown

        objectName: "showOnYAxis_" + rowKey
        checkable: false
        checked: shown
        highlighted: shown
        text: shown ? qsTr("✓ Shown on Y-axis") : qsTr("Show on Y-axis")
        Accessible.role: Accessible.RadioButton
        Accessible.checkable: true
        Accessible.checked: shown
        Accessible.name: qsTr("Show %1 on Y-axis").arg(rowName)
    }

    component AxisRow: Frame {
        id: axisRow

        required property string axisId
        required property string key
        required property string name
        required property string nameText
        required property string nameIssue
        required property int lineCount
        required property string lineNamesText
        required property bool hasEnabledLine
        required property bool shown
        readonly property bool narrow: width < 520
        readonly property alias nameField: nameInput
        readonly property alias deleteButton: deleteInput

        objectName: "axisRow_" + axisId
        Layout.fillWidth: true

        GridLayout {
            width: parent.width
            columns: axisRow.narrow ? 1 : 2
            columnSpacing: 8
            rowSpacing: 4

            TextField {
                id: nameInput
                objectName: "axisNameField_" + axisRow.axisId
                Layout.row: 0
                Layout.column: 0
                Layout.fillWidth: true
                text: axisRow.nameText
                Accessible.name: qsTr("Name of axis %1").arg(axisRow.name)
                Accessible.description: errorLabel.text
                // Typing breaks the binding; restoring it shows what the VM kept, trimmed or rejected.
                onEditingFinished: {
                    if (text !== axisRow.nameText)
                        root.settingsVm.renameAxis(axisRow.axisId, text)
                    text = Qt.binding(() => axisRow.nameText)
                }
            }

            Label {
                id: errorLabel
                objectName: "axisNameError_" + axisRow.axisId
                Layout.row: 1
                Layout.column: 0
                Layout.columnSpan: axisRow.narrow ? 1 : 2
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                text: AxisNames.message(axisRow.nameIssue)
                visible: text !== ""
                color: Theme.errorText
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.row: axisRow.narrow ? 2 : 0
                Layout.column: axisRow.narrow ? 0 : 1
                spacing: 8

                YAxisButton {
                    rowKey: axisRow.key
                    rowName: axisRow.name
                    shown: axisRow.shown
                    ButtonGroup.group: yAxisSelection
                    onClicked: root.chooseYAxis(rowKey, shown)
                }

                ActionButton {
                    id: deleteInput
                    objectName: "deleteAxis_" + axisRow.axisId
                    text: axisRow.lineCount > 0 ? qsTr("Delete…") : qsTr("Delete")
                    Accessible.name: qsTr("Delete axis %1").arg(axisRow.name)
                    onClicked: root.requestDelete(axisRow.axisId, axisRow.lineCount)
                }
            }

            Label {
                objectName: "axisUsage_" + axisRow.axisId
                Layout.row: 3
                Layout.column: 0
                Layout.columnSpan: axisRow.narrow ? 1 : 2
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                wrapMode: Text.WordWrap
                // The app ships no translations, so a bare "%n line(s)" would show its "(s)" verbatim.
                text: axisRow.lineCount === 0 ? qsTr("Not assigned to any lines.")
                    : axisRow.lineCount === 1 ? qsTr("Used by 1 line: %1").arg(axisRow.lineNamesText)
                    : qsTr("Used by %n lines: %1", "", axisRow.lineCount).arg(axisRow.lineNamesText)
            }

            Label {
                objectName: "axisHiddenNote_" + axisRow.axisId
                Layout.row: 4
                Layout.column: 0
                Layout.columnSpan: axisRow.narrow ? 1 : 2
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                visible: !axisRow.hasEnabledLine
                wrapMode: Text.WordWrap
                color: Qt.alpha(palette.text, 0.7)
                text: qsTr("No visible lines — the graph labels its first visible line instead.")
            }
        }
    }

    ColumnLayout {
        id: content
        width: root.availableWidth
        spacing: 6

        Label {
            objectName: "sharedAxesHeading"
            text: qsTr("Shared axes")
            font.bold: true
        }

        Repeater {
            id: axisRepeater
            model: ListModel {
                id: axisRows
            }
            delegate: AxisRow {}
        }

        ActionButton {
            id: newAxisButton
            objectName: "newAxisButton"
            text: qsTr("+ New axis…")
            onClicked: axisCreationDialog.openFor("")
        }

        Label {
            objectName: "independentLinesHeading"
            Layout.topMargin: 6
            text: qsTr("Independent lines")
            font.bold: true
        }

        Label {
            objectName: "noIndependentLinesLabel"
            Layout.fillWidth: true
            visible: root.independentLines.length === 0
            wrapMode: Text.WordWrap
            color: Qt.alpha(palette.text, 0.7)
            text: qsTr("Every line uses a shared axis.")
        }

        Repeater {
            id: independentRepeater
            model: ListModel {
                id: independentRows
            }

            Frame {
                id: independentRow

                required property string lineId
                required property string key
                required property string name
                required property bool lineEnabled
                required property bool shown
                readonly property bool narrow: width < 520

                objectName: "independentRow_" + lineId
                Layout.fillWidth: true

                GridLayout {
                    width: parent.width
                    columns: independentRow.narrow ? 1 : 2
                    columnSpacing: 8
                    rowSpacing: 4

                    ColumnLayout {
                        Layout.row: 0
                        Layout.column: 0
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            objectName: "independentName_" + independentRow.lineId
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            wrapMode: Text.WordWrap
                            text: independentRow.name
                        }

                        Label {
                            objectName: "independentHiddenNote_" + independentRow.lineId
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            visible: !independentRow.lineEnabled
                            wrapMode: Text.WordWrap
                            color: Qt.alpha(palette.text, 0.7)
                            text: qsTr("Hidden line — the graph labels its first visible line instead.")
                        }
                    }

                    YAxisButton {
                        Layout.row: independentRow.narrow ? 1 : 0
                        Layout.column: independentRow.narrow ? 0 : 1
                        rowKey: independentRow.key
                        rowName: independentRow.name
                        shown: independentRow.shown
                        ButtonGroup.group: yAxisSelection
                        onClicked: root.chooseYAxis(rowKey, shown)
                    }
                }
            }
        }
    }

    // ---- Dialogs -------------------------------------------------------------------------------------

    AxisNameDialog {
        id: axisCreationDialog
        settingsVm: root.settingsVm
        onCreated: axisId => root.createdAxisId = axisId
        onClosed: {
            const row = root.createdAxisId !== "" ? root.axisRowFor(root.createdAxisId) : null
            root.createdAxisId = ""
            if (row)
                row.nameField.forceActiveFocus(Qt.TabFocusReason)
            else
                newAxisButton.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    Dialog {
        id: deleteAxisDialog

        readonly property var lineNames: root.deleteTarget ? root.deleteTarget.lineNames : []

        objectName: "deleteAxisDialog"
        modal: true
        focus: true
        anchors.centerIn: Overlay.overlay
        title: root.deleteTarget ? qsTr("Delete \"%1\" axis?").arg(root.deleteTarget.name) : ""
        onOpened: deleteCancelButton.forceActiveFocus(Qt.TabFocusReason)
        onClosed: {
            const row = root.deleteConfirmed ? null : root.axisRowFor(root.deleteTargetId)
            root.deleteConfirmed = false
            root.deleteTargetId = ""
            if (row)
                row.deleteButton.forceActiveFocus(Qt.TabFocusReason)
            else
                newAxisButton.forceActiveFocus(Qt.TabFocusReason)
        }

        contentItem: ColumnLayout {
            spacing: 6

            Label {
                objectName: "deleteAxisSummary"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 320
                wrapMode: Text.WordWrap
                text: deleteAxisDialog.lineNames.length === 1 ? qsTr("1 line will switch to Independent:")
                    : qsTr("%n lines will switch to Independent:", "", deleteAxisDialog.lineNames.length)
            }

            Repeater {
                model: deleteAxisDialog.lineNames

                Label {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.leftMargin: 12
                    wrapMode: Text.WordWrap
                    text: "• " + modelData
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.topMargin: 6
                wrapMode: Text.WordWrap
                text: qsTr("Each line will use its own scale.")
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                wrapMode: Text.WordWrap
                text: qsTr("The lines and their data will be kept.")
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.topMargin: 6
                wrapMode: Text.WordWrap
                color: Qt.alpha(palette.text, 0.7)
                text: qsTr("Changes take effect when you save Settings.")
            }
        }

        footer: DialogButtonBox {
            ActionButton {
                id: deleteCancelButton
                objectName: "deleteAxisCancelButton"
                text: qsTr("Cancel")
                onClicked: deleteAxisDialog.close()
            }
            ActionButton {
                objectName: "deleteAxisConfirmButton"
                text: qsTr("Delete axis")
                highlighted: true
                onClicked: root.confirmDelete()
            }
        }
    }
}
