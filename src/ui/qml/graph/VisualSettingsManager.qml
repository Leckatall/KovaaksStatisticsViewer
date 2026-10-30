import QtQuick
import QtCore
import KovaaksStatsViewer

Item {
    id: root
    visible: false

    property alias windowWidth: windowSettings.width
    property alias windowHeight: windowSettings.height
    property alias windowX: windowSettings.x
    property alias windowY: windowSettings.y
    property alias windowHasPosition: windowSettings.hasPosition
    Settings {
        id: windowSettings
        category: "window"
        property int width: 1200
        property int height: 800
        // No position fits every monitor layout, so until one is saved the OS places the window.
        property bool hasPosition: false
        property int x: 0
        property int y: 0
    }

    // A saved position can point at a monitor that has since been unplugged or rearranged; restoring
    // it is only safe while enough of the window's top edge is on some screen to drag it back.
    function isVisibleOnScreens(x, y, width, height, screens) {
        const margin = 100
        for (const screen of screens) {
            const overlap = Math.min(x + width, screen.virtualX + screen.width) - Math.max(x, screen.virtualX)
            if (overlap >= Math.min(margin, width)
                    && y >= screen.virtualY && y <= screen.virtualY + screen.height - margin)
                return true
        }
        return false
    }

    property alias scenarioGraphVisible: panelSettings.scenarioGraphVisible
    property alias playtimeGraphVisible: panelSettings.playtimeGraphVisible
    property alias scenarioHistoryGraphVisible: panelSettings.scenarioHistoryGraphVisible
    property alias benchmarkRankGraphVisible: panelSettings.benchmarkRankGraphVisible
    property alias benchmarkRecentRankGraphVisible: panelSettings.benchmarkRecentRankGraphVisible
    property alias benchmarkPlaytimeGraphVisible: panelSettings.benchmarkPlaytimeGraphVisible
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
        property bool benchmarkRankGraphVisible: true
        property bool benchmarkRecentRankGraphVisible: true
        property bool benchmarkPlaytimeGraphVisible: true
        property bool controlPanelVisible: true
        property bool selectionPanelVisible: true
        property bool recentRunsSectionVisible: true
        property bool scenarioBrowserSectionVisible: true
    }

    readonly property var graphVisibilityProperties: ({
        run: "scenarioGraphVisible",
        dailyPlaytime: "playtimeGraphVisible",
        scenarioHistory: "scenarioHistoryGraphVisible",
        benchmarkRank: "benchmarkRankGraphVisible",
        benchmarkRecentRank: "benchmarkRecentRankGraphVisible",
        benchmarkPlaytime: "benchmarkPlaytimeGraphVisible"
    })

    function isGraphVisible(graphKey) {
        return root[root.graphVisibilityProperties[graphKey]]
    }

    function setGraphVisible(graphKey, shown) {
        root[root.graphVisibilityProperties[graphKey]] = shown
    }

    // The graph a quick hide's Undo/Dismiss notice is standing in for. Session feedback, so it stays
    // outside every Settings block: a notice never returns after a restart.
    property string hideNoticeGraph: ""

    function quickHideGraph(graphKey) {
        root.setGraphVisible(graphKey, false)
        root.hideNoticeGraph = graphKey
    }

    function undoQuickHide() {
        if (root.hideNoticeGraph !== "") root.setGraphVisible(root.hideNoticeGraph, true)
    }

    function dismissHideNotice() {
        root.hideNoticeGraph = ""
    }

    // Otherwise a graph shown from elsewhere and hidden again would bring its old notice back.
    function clearNoticeOfShownGraph() {
        if (root.hideNoticeGraph !== "" && root.isGraphVisible(root.hideNoticeGraph)) root.hideNoticeGraph = ""
    }
    onScenarioGraphVisibleChanged: clearNoticeOfShownGraph()
    onPlaytimeGraphVisibleChanged: clearNoticeOfShownGraph()
    onScenarioHistoryGraphVisibleChanged: clearNoticeOfShownGraph()
    onBenchmarkRankGraphVisibleChanged: clearNoticeOfShownGraph()
    onBenchmarkRecentRankGraphVisibleChanged: clearNoticeOfShownGraph()
    onBenchmarkPlaytimeGraphVisibleChanged: clearNoticeOfShownGraph()

    property alias historyColumnVisibility: historySettings.columnVisibility
    property alias historyAxisColumnKey: historySettings.yAxisColumnKey
    Settings {
        id: historySettings
        category: "historyGraph"
        property var columnVisibility: ({"1": true, "2": false, "3": false, "4": false, "5": false, "6": true})
        property string yAxisColumnKey: "1"
    }

    // Maps saved before the average existed have no "6" entry; it defaults to shown.
    function isHistorySeriesVisible(id) {
        const values = root.historyColumnVisibility
        return Object.prototype.hasOwnProperty.call(values, id) ? !!values[id] : id === "6"
    }

    function setHistorySeriesVisible(id, shown) {
        const next = Object.assign({}, root.historyColumnVisibility)
        next[id] = shown
        root.historyColumnVisibility = next
    }

    property alias visibleSeriesIds: visibilitySettings.visibleSeriesIds
    property alias seenSeriesIds: visibilitySettings.seenSeriesIds
    Settings {
        id: visibilitySettings
        category: "graphSeriesVisibility"
        property var visibleSeriesIds: []
        property var seenSeriesIds: []
    }

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
        const seen = new Set(root.seenSeriesIds)
        const current = root.visibleSeriesIds
        const result = []
        for (const id of enabledIds) {
            if (seen.has(id)) {
                if (current.indexOf(id) !== -1) result.push(id)
            } else {
                result.push(id)
            }
        }
        root.visibleSeriesIds = result
        root.seenSeriesIds = enabledIds
    }
}
