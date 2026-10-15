import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer

// Replacement drops the previous versions; linking preserves them.
Pane {
    id: mappingPanel
    objectName: "mappingPanel"
    padding: 4

    property var currentRow: null
    property var scenarioCatalogue: []

    signal mappingEditRequested(string entryId, string hash)
    signal linkedVersionRequested(string hash, bool linked)

    property bool candidatesArmed: false
    property bool linking: false
    readonly property string currentEntryId: mappingPanel.currentRow && mappingPanel.currentRow.entryId ? mappingPanel.currentRow.entryId : ""
    readonly property string mappingState: mappingPanel.currentRow && mappingPanel.currentRow.mappingState ? mappingPanel.currentRow.mappingState : "unresolved"
    readonly property var candidates: mappingPanel.currentRow && mappingPanel.currentRow.mappingCandidates ? mappingPanel.currentRow.mappingCandidates : []
    readonly property var mappedHashes: mappingPanel.currentRow && mappingPanel.currentRow.mappedHashes ? mappingPanel.currentRow.mappedHashes : []
    readonly property bool armed: mappingPanel.candidatesArmed || mappingPanel.mappingState === "ambiguous"

    onCurrentEntryIdChanged: {
        mappingPanel.candidatesArmed = false
        mappingPanel.linking = false
    }

    function matchStateLabel(state) {
        return ({
            resolved: qsTr("Resolved"),
            mappedUnavailable: qsTr("Mapped, unavailable"),
            unresolved: qsTr("Unresolved"),
            ambiguous: qsTr("Ambiguous"),
            autoMappable: qsTr("Pending automatic mapping")
        })[state] || qsTr("Unresolved")
    }

    function offersMappingEdit(state) {
        return state === "resolved" || state === "mappedUnavailable"
    }

    function candidateText(candidate) {
        const parts = [String(candidate.hash),
                       qsTr("%n run(s)", "", candidate.runCount || 0)]
        const played = candidate.lastPlayed
        if (played && !isNaN(new Date(played).getTime()))
            parts.push(Qt.formatDate(played, "yyyy-MM-dd"))
        return parts.join(" · ")
    }

    ColumnLayout {
        width: parent.width
        spacing: 2
        Label {
            objectName: "mappingHint"
            Layout.fillWidth: true
            Layout.preferredHeight: clearMappingButton.implicitHeight
            visible: mappingPanel.currentEntryId === ""
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: qsTr("Select a scenario to see its mapping")
            color: mappingPanel.palette.placeholderText
        }
        // Wraps instead of running past a narrow editor, so the buttons stay reachable.
        Flow {
            Layout.fillWidth: true
            visible: mappingPanel.currentEntryId !== ""
            spacing: 8
            Label {
                height: clearMappingButton.implicitHeight
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Mapping for %1:").arg(mappingPanel.currentRow && mappingPanel.currentRow.name ? mappingPanel.currentRow.name : qsTr("(unnamed)"))
            }
            Label {
                objectName: "matchLabel"
                height: clearMappingButton.implicitHeight
                verticalAlignment: Text.AlignVCenter
                text: mappingPanel.matchStateLabel(mappingPanel.mappingState)
                color: mappingPanel.palette.placeholderText
            }
            Button {
                objectName: "changeMappingButton"
                text: mappingPanel.mappedHashes.length > 1 ? qsTr("Replace mappings") : qsTr("Change mapping")
                visible: mappingPanel.offersMappingEdit(mappingPanel.mappingState)
                Accessible.name: qsTr("Change the scenario mapping")
                onClicked: {
                    mappingPanel.linking = false
                    mappingPanel.candidatesArmed = !mappingPanel.candidatesArmed
                }
            }
            Button {
                objectName: "linkVersionsButton"
                text: qsTr("Link versions")
                checkable: true
                checked: mappingPanel.linking
                enabled: mappingPanel.linking
                    || mappingPanel.candidates.some(candidate => mappingPanel.mappedHashes.indexOf(candidate.hash) < 0)
                Accessible.name: qsTr("Link another version with the same scenario name")
                onClicked: {
                    mappingPanel.linking = !mappingPanel.linking
                    mappingPanel.candidatesArmed = false
                }
            }
            Button {
                id: clearMappingButton
                objectName: "clearMappingButton"
                text: qsTr("Clear mapping")
                visible: mappingPanel.offersMappingEdit(mappingPanel.mappingState)
                Accessible.name: qsTr("Clear the scenario mapping")
                onClicked: mappingPanel.mappingEditRequested(mappingPanel.currentEntryId, "")
            }
        }
        Repeater {
            model: mappingPanel.mappedHashes
            delegate: RowLayout {
                id: linkedVersion
                required property string modelData
                Layout.fillWidth: true
                readonly property var candidate: mappingPanel.candidates.find(value => value.hash === linkedVersion.modelData)
                readonly property bool available: linkedVersion.candidate !== undefined
                    || (mappingPanel.scenarioCatalogue || []).some(value => value.hash === linkedVersion.modelData)
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: linkedVersion.candidate ? mappingPanel.candidateText(linkedVersion.candidate)
                        : (linkedVersion.available ? linkedVersion.modelData
                            : qsTr("%1 · unavailable").arg(linkedVersion.modelData))
                }
                Button {
                    objectName: "unlinkVersion_" + linkedVersion.modelData
                    text: qsTr("Unlink")
                    Accessible.name: qsTr("Unlink scenario version %1").arg(linkedVersion.modelData)
                    onClicked: mappingPanel.linkedVersionRequested(linkedVersion.modelData, false)
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: mappingPanel.linking
            text: qsTr("Choose a version to include its scores and history in this benchmark row.")
            wrapMode: Text.WordWrap
            color: mappingPanel.palette.placeholderText
        }
        Repeater {
            model: mappingPanel.currentEntryId !== "" ? mappingPanel.candidates : []
            delegate: ItemDelegate {
                id: candidateOption
                required property var modelData
                objectName: "candidateOption_" + candidateOption.modelData.hash
                Layout.fillWidth: true
                visible: mappingPanel.candidatesArmed
                    || mappingPanel.mappedHashes.indexOf(candidateOption.modelData.hash) < 0
                enabled: mappingPanel.linking
                    ? mappingPanel.mappedHashes.indexOf(candidateOption.modelData.hash) < 0
                    : mappingPanel.armed
                opacity: enabled ? 1 : 0.4
                text: mappingPanel.candidateText(candidateOption.modelData)
                Accessible.name: mappingPanel.linking
                    ? qsTr("Link version %1").arg(candidateOption.text)
                    : qsTr("Map to %1").arg(candidateOption.text)
                onClicked: {
                    if (mappingPanel.linking) mappingPanel.linkedVersionRequested(candidateOption.modelData.hash, true)
                    else mappingPanel.mappingEditRequested(mappingPanel.currentEntryId, candidateOption.modelData.hash)
                }
            }
        }
    }
}
