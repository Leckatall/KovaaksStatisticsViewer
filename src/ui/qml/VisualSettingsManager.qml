import QtQuick
import QtCore

Item {
    id: root
    visible: false

    property alias windowWidth: windowSettings.width
    property alias windowHeight: windowSettings.height
    Settings {
        id: windowSettings
        category: "window"
        property int width: 1200
        property int height: 800
    }

    property alias scenarioGraphVisible: panelSettings.scenarioGraphVisible
    property alias playtimeGraphVisible: panelSettings.playtimeGraphVisible
    property alias scenarioHistoryGraphVisible: panelSettings.scenarioHistoryGraphVisible
    property alias controlPanelVisible: panelSettings.controlPanelVisible
    property alias selectionPanelVisible: panelSettings.selectionPanelVisible
    property alias recentRunsSectionVisible: panelSettings.recentRunsSectionVisible
    property alias scenarioBrowserSectionVisible: panelSettings.scenarioBrowserSectionVisible
    Settings {
        id: panelSettings
        category: "view"
        property bool scenarioGraphVisible: true
        property bool playtimeGraphVisible: true
        property bool scenarioHistoryGraphVisible: true
        property bool controlPanelVisible: true
        property bool selectionPanelVisible: true
        property bool recentRunsSectionVisible: true
        property bool scenarioBrowserSectionVisible: true
    }

    property alias graphAxisSeriesId: axisSettings.seriesId
    Settings {
        id: axisSettings
        category: "graphAxis"
        property string seriesId: ""
    }

    property alias historyColumnVisibility: historySettings.columnVisibility
    property alias historyAxisColumnKey: historySettings.yAxisColumnKey
    Settings {
        id: historySettings
        category: "historyGraph"
        property var columnVisibility: ({"score": true, "accuracy": false, "shots": false, "hits": false, "misses": false})
        property string yAxisColumnKey: "score"
    }

    property alias visibleSeriesIds: visibilitySettings.visibleSeriesIds
    Settings {
        id: visibilitySettings
        category: "graphSeriesVisibility"
        property var visibleSeriesIds: []
    }

    property var seenIds: new Set()

    function isSeriesVisible(id) {
        return root.visibleSeriesIds.indexOf(id) !== -1
    }

    function setSeriesVisible(id, visible) {
        const current = root.visibleSeriesIds
        const index = current.indexOf(id)
        if (visible && index === -1) {
            root.visibleSeriesIds = current.concat([id])
        } else if (!visible && index !== -1) {
            const next = current.slice()
            next.splice(index, 1)
            root.visibleSeriesIds = next
        }
    }

    function syncVisibleSeriesIds(enabledIds) {
        const current = root.visibleSeriesIds
        const result = []
        for (const id of enabledIds) {
            if (root.seenIds.has(id)) {
                if (current.indexOf(id) !== -1) result.push(id)
            } else {
                result.push(id)
            }
        }
        root.visibleSeriesIds = result
        root.seenIds = new Set(enabledIds)
    }
}
