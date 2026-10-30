.pragma library

// Display names of the configurable graphs, in the order the Graphs settings page lists them.
const graphs = [
    {key: "run", name: "Run graph", workspace: "Scenarios"},
    {key: "dailyPlaytime", name: "Daily playtime", workspace: "Scenarios"},
    {key: "scenarioHistory", name: "Scenario history", workspace: "Scenarios"},
    {key: "benchmarkRank", name: "Benchmark rank history", workspace: "Benchmarks"},
    {key: "benchmarkRecentRank", name: "Benchmark recent rank", workspace: "Benchmarks"},
    {key: "benchmarkPlaytime", name: "Benchmark playtime", workspace: "Benchmarks"}
]

function isKnown(key) {
    return graphs.some(graph => graph.key === key)
}

function nameOf(key) {
    const found = graphs.find(graph => graph.key === key)
    return found ? found.name : key
}

function workspaceOf(key) {
    const found = graphs.find(graph => graph.key === key)
    return found ? found.workspace : ""
}

// ---- Shared by the gear popup and the Graphs page ----------------------------------------------

const connectionModes = [
    {text: qsTr("Smooth lines"), value: "smooth"},
    {text: qsTr("Straight lines"), value: "straight"},
    {text: qsTr("Points only"), value: "pointsOnly"}
]

const recentDayPresets = [7, 30, 90]
const latestRunPresets = [25, 50, 100]

function allHistoryText(offersLatestRuns) {
    return offersLatestRuns ? qsTr("All runs") : qsTr("All time")
}

function wholeNumberText(value) {
    return Number(value).toLocaleString(Qt.locale(), "f", 0)
}

// ---- Detailed-settings vocabulary --------------------------------------------------------------

const groupNames = {
    dataRange: "Data & range",
    axes: "Axes",
    seriesStyle: "Lines & style"
}

// The order the groups appear on the Graphs page; an issue list reads the same way.
const groupOrder = ["dataRange", "seriesStyle", "axes"]

const seriesNames = {
    scenarioHistory: {score: "Score", accuracy: "Accuracy", shots: "Shots", hits: "Hits", misses: "Misses", scoreAverage: "Score average (10 runs)"},
    dailyPlaytime: {playtime: "Playtime"},
    benchmarkPlaytime: {playtime: "Playtime"},
    benchmarkRank: {rank: "Personal-best average rank"},
    benchmarkRecentRank: {rank: "Recent average rank"}
}

// Bounds are typed in the unit the axis shows: accuracy as a percentage, playtime in minutes.
const axisInfo = {
    scenarioHistory: {
        score: {name: "Score", unit: ""},
        accuracy: {name: "Accuracy", unit: "%"},
        counts: {name: "Shots, hits and misses", unit: ""}
    },
    dailyPlaytime: {playtime: {name: "Playtime", unit: "minutes"}},
    benchmarkPlaytime: {playtime: {name: "Playtime", unit: "minutes"}}
}

function graphIndex(key) {
    return graphs.findIndex(graph => graph.key === key)
}

function seriesName(graph, key) {
    const names = seriesNames[graph]
    return names && names[key] ? names[key] : key
}

function axisName(graph, key) {
    const axes = axisInfo[graph]
    return axes && axes[key] ? axes[key].name : key
}

function axisUnit(graph, key) {
    const axes = axisInfo[graph]
    return axes && axes[key] ? axes[key].unit : ""
}

// Issues in the order a person reads the pages: graph, then group, then position within the group.
function sortedIssues(issues, drafts, overview) {
    function sortKey(issue) {
        const issueTarget = issue.target || ""
        const axisName = issue.group === "graphLines" && issueTarget.startsWith("axis:") && issue.field === "name"
        const group = issue.group === "graphLines" ? (axisName ? "axes" : "seriesStyle") : issue.group
        const graph = graphIndex(issue.graph)
        const section = groupOrder.indexOf(group)
        const draft = (drafts || {})[issue.graph] || {}
        let row = 0
        let field = 0
        if (group === "dataRange") {
            field = ["range", "averagingWindow", "recentDays", "latestRuns", "startDate", "endDate", "anchor"].indexOf(issue.field)
        } else if (group === "seriesStyle") {
            const common = ["connection", "markers"].indexOf(issue.field)
            if (common >= 0) field = common
            else {
                const index = (draft.series || []).findIndex(r => r.key === issueTarget)
                row = issue.group === "graphLines" ? 1 : (index < 0 ? 999 : index + 1)
                field = ["color", "width"].indexOf(issue.field)
            }
        } else if (group === "axes") {
            const rows = axisName ? ((overview || {}).sharedAxes || []) : (draft.axes || [])
            const target = axisName ? issueTarget.substring(5) : issueTarget
            const index = rows.findIndex(r => String(r.id !== undefined ? r.id : r.key) === target)
            row = index < 0 ? 999 : index
            field = axisName ? 0 : ["fitMode", "fixedMin", "fixedMax"].indexOf(issue.field || "fitMode")
        }
        return [graph < 0 ? 999 : graph, section < 0 ? 999 : section, row, field < 0 ? 999 : field]
    }
    return issues.map((issue, position) => ({issue: issue, position: position, key: sortKey(issue)}))
        .sort((a, b) => {
            for (let i = 0; i < a.key.length; ++i)
                if (a.key[i] !== b.key[i]) return a.key[i] - b.key[i]
            return a.position - b.position
        }).map(entry => entry.issue)
}

// objectName of the control an issue is about; empty when the issue names no control.
function controlName(issue) {
    if (issue.group === "graphLines") return ""
    const field = issue.field === "" ? (issue.group === "axes" ? "fitMode" : "") : issue.field
    const parts = ["graphField", issue.group]
    if (issue.target !== "") parts.push(issue.target)
    if (field !== "") parts.push(field)
    return parts.length > 2 ? parts.join("_") : ""
}

function findByObjectName(root, name) {
    if (!root) return null
    if (root.objectName === name) return root
    for (const child of root.children || []) {
        const found = findByObjectName(child, name)
        if (found) return found
    }
    return null
}

function isWithin(item, ancestor) {
    for (let current = item; current; current = current.parent)
        if (current === ancestor) return true
    return false
}

// Scrolls `flick` the least distance that shows `item` with `margin` clear above and below it.
function ensureVisible(flick, item, margin) {
    // Unsized until its layout's first polish, when everything looks out of view.
    if (flick.height <= 0) return
    const top = item.mapToItem(flick.contentItem, 0, 0).y
    const bottom = top + item.height
    if (top - margin < flick.contentY)
        flick.contentY = Math.max(0, top - margin)
    else if (bottom + margin > flick.contentY + flick.height)
        flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, bottom + margin - flick.height))
}
