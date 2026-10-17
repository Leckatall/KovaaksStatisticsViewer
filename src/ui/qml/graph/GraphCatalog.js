.pragma library

// Display names of the five configurable graphs, in the order the Graphs settings page lists them.
const graphs = [
    {key: "run", name: "Run graph"},
    {key: "dailyPlaytime", name: "Daily playtime"},
    {key: "scenarioHistory", name: "Scenario history"},
    {key: "benchmarkRank", name: "Benchmark rank history"},
    {key: "benchmarkPlaytime", name: "Benchmark playtime"}
]

function isKnown(key) {
    return graphs.some(graph => graph.key === key)
}

function nameOf(key) {
    const found = graphs.find(graph => graph.key === key)
    return found ? found.name : key
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
    seriesStyle: "Series & style",
    graphLines: "Graph Lines"
}

// The order the groups appear on the Graphs page; an issue list reads the same way.
const groupOrder = ["dataRange", "axes", "seriesStyle", "graphLines"]

const seriesNames = {
    scenarioHistory: {score: "Score", accuracy: "Accuracy", shots: "Shots", hits: "Hits", misses: "Misses"},
    dailyPlaytime: {playtime: "Playtime"},
    benchmarkPlaytime: {playtime: "Playtime"},
    benchmarkRank: {rank: "Personal-best average rank"}
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

function groupName(group) {
    return groupNames[group] || group
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
function sortedIssues(issues) {
    const rank = issue => graphIndex(issue.graph) * 10 + Math.max(0, groupOrder.indexOf(issue.group))
    return issues.map((issue, position) => ({issue: issue, position: position}))
        .sort((a, b) => rank(a.issue) - rank(b.issue) || a.position - b.position)
        .map(entry => entry.issue)
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
