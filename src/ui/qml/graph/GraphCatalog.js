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
