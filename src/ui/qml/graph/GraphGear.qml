import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KovaaksStatsViewer
import "GraphCatalog.js" as GraphCatalog

// A graph's gear and the quick-settings popup it opens. Every change goes straight to the committed
// configuration through settingsVm.applyQuickPatch(); nothing here is a draft. The gear belongs in the
// graph's header, outside any loader that needs plottable data, so it stays reachable when the graph
// is empty.
TooltipToolButton {
    id: gear

    // One of run, dailyPlaytime, scenarioHistory, benchmarkRank, benchmarkPlaytime.
    required property string graphKey
    // The SettingsViewModel; the popup still opens, with defaults, without one.
    property var settingsVm: null

    readonly property alias popup: quickPopup

    // Emitted after the popup has closed.
    signal moreSettingsRequested(string graphKey)

    readonly property string graphName: GraphCatalog.nameOf(graphKey)

    // The committed values of this graph; empty when there is no view model.
    readonly property var quick: settingsVm && settingsVm.quickSettings && settingsVm.quickSettings[graphKey]
                                 ? settingsVm.quickSettings[graphKey] : ({})

    readonly property int maximumWindowDays: 3650
    readonly property int maximumLatestRuns: 1000000

    property string windowError: ""
    property string latestRunsError: ""

    function pluralised(count, singular, plural) {
        return count === 1 ? singular : plural.arg(quick.rangeCountText)
    }

    function limitText(limit) {
        return Number(limit).toLocaleString(Qt.locale(), "f", 0)
    }

    readonly property bool offersRange: quick.hasRange === true
    readonly property bool offersLatestRuns: quick.hasLatestRuns === true

    readonly property var rangePresets: {
        if (!gear.offersRange) return []
        const presets = []
        const add = (text, kind, count) => presets.push({
            text: text, kind: kind, count: count,
            patch: kind === "allHistory" ? {rangeKind: kind} : {rangeKind: kind, count: count}
        })
        for (const days of [7, 30, 90]) add(qsTr("Last %1 days").arg(days), "recentDays", days)
        if (gear.offersLatestRuns)
            for (const runs of [25, 50, 100]) add(qsTr("Latest %1 runs").arg(runs), "latestRuns", runs)
        add(gear.offersLatestRuns ? qsTr("All runs") : qsTr("All time"), "allHistory", 0)
        return presets
    }

    // -1 when the committed range is no preset (set from the settings page); rangeText still says
    // what it is, so the popup never misreports it as a preset.
    readonly property int rangeIndex: rangePresets.findIndex(
        p => p.kind === quick.rangeKind && (p.kind === "allHistory" || p.count === quick.rangeCount))

    readonly property string rangeText: {
        if (rangeIndex >= 0) return rangePresets[rangeIndex].text
        if (quick.rangeKind === "customDates")
            return qsTr("Custom: %1 – %2").arg(quick.rangeStartFormatted).arg(quick.rangeEndFormatted)
        if (quick.rangeKind === "latestRuns")
            return pluralised(quick.rangeCount, qsTr("Latest 1 run"), qsTr("Latest %1 runs"))
        return pluralised(quick.rangeCount, qsTr("Last 1 day"), qsTr("Last %1 days"))
    }

    readonly property var connectionEntries: [
        {text: qsTr("Smooth lines"), value: "smooth"},
        {text: qsTr("Straight lines"), value: "straight"},
        {text: qsTr("Points only"), value: "pointsOnly"}
    ]

    function apply(patch) {
        return gear.settingsVm ? gear.settingsVm.applyQuickPatch(gear.graphKey, patch)
                               : {accepted: false, issues: []}
    }

    function errorFor(result, limitText) {
        if (result.accepted) return ""
        return qsTr("Enter a whole number from 1 to %1.").arg(limitText)
    }

    // Text fields follow the committed value unless the person is typing in them.
    function syncFields(force) {
        if (force || !windowField.activeFocus)
            windowField.text = gear.quick.hasWindow === true ? gear.quick.windowText : ""
        if (force || !latestRunsField.activeFocus)
            latestRunsField.text = gear.quick.rangeKind === "latestRuns" ? gear.quick.rangeCountText : ""
    }

    onQuickChanged: syncFields(false)

    function focusFirstControl() {
        for (const candidate of [rangeCombo, windowField, connectionCombo]) {
            if (candidate.visible && candidate.enabled) {
                candidate.forceActiveFocus()
                return
            }
        }
    }

    objectName: "graphGear_" + graphKey
    text: qsTr("%1 settings").arg(graphName)
    icon.source: "../icons/settings.svg"
    icon.width: 18
    icon.height: 18
    display: AbstractButton.IconOnly
    padding: 4
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    ToolTip.text: text

    onClicked: quickPopup.opened ? quickPopup.close() : quickPopup.open()

    Popup {
        id: quickPopup
        objectName: "graphQuickPopup_" + gear.graphKey

        parent: gear
        x: gear.width - width
        y: gear.height + 4
        width: 300
        padding: 12
        margins: 8
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        onAboutToShow: {
            gear.windowError = ""
            gear.latestRunsError = ""
            gear.syncFields(true)
        }
        onOpened: gear.focusFirstControl()
        onClosed: gear.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: 8

            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("%1 quick settings").arg(gear.graphName)

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: gear.graphName
                    font.bold: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    objectName: "graphQuickClose_" + gear.graphKey
                    text: "×"
                    padding: 4
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: qsTr("Close")
                    onClicked: quickPopup.close()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: gear.quick.hasRange === true

                Label { text: qsTr("Show") }
                ComboBox {
                    id: rangeCombo
                    objectName: "graphField_dataRange_range"
                    Layout.fillWidth: true
                    model: gear.rangePresets
                    textRole: "text"
                    currentIndex: gear.rangeIndex
                    displayText: gear.rangeText
                    Accessible.name: qsTr("Visible range")
                    onActivated: index => {
                        gear.apply(gear.rangePresets[index].patch)
                        rangeCombo.currentIndex = Qt.binding(() => gear.rangeIndex)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: gear.quick.hasLatestRuns === true

                Label { text: qsTr("Latest runs") }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: latestRunsField
                        objectName: "graphField_dataRange_latestRuns"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Custom count")
                        Accessible.name: qsTr("Latest runs, custom count")
                        Accessible.description: gear.latestRunsError !== "" ? gear.latestRunsError
                            : qsTr("Whole runs, 1 to %1.").arg(gear.limitText(gear.maximumLatestRuns))
                        onTextEdited: {
                            gear.latestRunsError = gear.errorFor(
                                gear.apply({rangeKind: "latestRuns", count: text}), gear.limitText(gear.maximumLatestRuns))
                        }
                    }
                    Label { text: qsTr("runs") }
                }
                Label {
                    objectName: "graphField_dataRange_latestRuns_error"
                    Accessible.role: Accessible.AlertMessage
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: gear.latestRunsError
                    color: Theme.errorText
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: gear.quick.hasWindow === true

                Label { text: qsTr("Rolling average") }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: windowField
                        objectName: "graphField_dataRange_averagingWindow"
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Rolling average window, in days")
                        Accessible.description: gear.windowError !== "" ? gear.windowError
                            : qsTr("Whole days, 1 to %1.").arg(gear.limitText(gear.maximumWindowDays))
                        onTextEdited: {
                            gear.windowError = gear.errorFor(gear.apply({window: text}), gear.limitText(gear.maximumWindowDays))
                        }
                    }
                    Label { text: qsTr("days") }
                }
                Label {
                    objectName: "graphField_dataRange_averagingWindow_error"
                    Accessible.role: Accessible.AlertMessage
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: gear.windowError
                    color: Theme.errorText
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label { text: qsTr("Line style") }
                ComboBox {
                    id: connectionCombo
                    objectName: "graphField_seriesStyle_connection"
                    Layout.fillWidth: true
                    model: gear.connectionEntries
                    textRole: "text"
                    currentIndex: Math.max(0, gear.connectionEntries.findIndex(e => e.value === gear.quick.connection))
                    Accessible.name: qsTr("Line style")
                    onActivated: index => {
                        gear.apply({connection: gear.connectionEntries[index].value})
                        connectionCombo.currentIndex = Qt.binding(
                            () => Math.max(0, gear.connectionEntries.findIndex(e => e.value === gear.quick.connection)))
                    }
                }
            }

            CheckBox {
                id: markerBox
                objectName: "graphField_seriesStyle_markers"

                readonly property bool pointsOnly: gear.quick.connection === "pointsOnly"

                text: qsTr("Show point markers")
                checked: pointsOnly || gear.quick.markers === true
                enabled: !pointsOnly
                Accessible.name: text
                Accessible.description: pointsOnly ? qsTr("Always shown while Points only is selected") : ""
                onToggled: {
                    gear.apply({markers: markerBox.checked})
                    markerBox.checked = Qt.binding(() => markerBox.pointsOnly || gear.quick.markers === true)
                }
            }

            Button {
                objectName: "graphMoreSettings_" + gear.graphKey
                Layout.fillWidth: true
                flat: true
                text: qsTr("More graph settings…")
                Accessible.name: qsTr("More settings for %1").arg(gear.graphName)
                onClicked: {
                    quickPopup.close()
                    gear.moreSettingsRequested(gear.graphKey)
                }
            }
        }
    }
}
