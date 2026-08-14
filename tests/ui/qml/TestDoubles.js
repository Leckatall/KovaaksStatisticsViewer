// .pragma library

function makeFakeGraphVm() {
    return {
        plottableColumns: [1, 2],
        allColumns: [1, 2],
        enabledColumns: [1, 2],
        fetchDataCalls: [],
        fetchLatestDataCalls: 0,
        columnName: function (id) { return id === 1 ? "Score" : "Accuracy" },
        columnKey: function (id) { return id === 1 ? "score" : "accuracy" },
        columnColor: function () { return "#00ff00" },
        fetchData: function (scenarioId) { this.fetchDataCalls.push(scenarioId) },
        fetchLatestData: function () { this.fetchLatestDataCalls++ }
    }
}

function makeFakeHistoryVm() {
    return {
        scenarioTitle: "Air Angelic",
        runCount: 0,
        columnName: function (id) {
            const names = ["Run", "Score", "Accuracy", "Shots", "Hits", "Misses"]
            return names[id]
        },
        columnKey: function (id) {
            const keys = ["run", "score", "accuracy", "shots", "hits", "misses"]
            return keys[id]
        }
    }
}
