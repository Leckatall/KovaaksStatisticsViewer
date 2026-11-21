import QtQuick
import KovaaksStatsViewer

// One choice of an exclusive "which row is shown on this axis" selection; whoever uses it sets
// ButtonGroup.group. Checked state follows `shown`, never a click, so revert and Cancel restore it;
// the click only reports the key and the owner's next `shown` carries the new selection.
ActionButton {
    id: root

    // "X" or "Y".
    required property string axisLetter
    required property string rowKey
    required property string rowName
    required property bool shown

    signal chosen(string key)

    objectName: "showOn" + axisLetter + "Axis_" + rowKey
    checkable: false
    checked: shown
    highlighted: shown
    text: shown ? qsTr("✓ Shown on %1-axis").arg(axisLetter) : qsTr("Show on %1-axis").arg(axisLetter)
    Accessible.role: Accessible.RadioButton
    Accessible.checkable: true
    Accessible.checked: shown
    Accessible.name: qsTr("Show %1 on %2-axis").arg(rowName).arg(axisLetter)
    onClicked: if (!shown) chosen(rowKey)
}
