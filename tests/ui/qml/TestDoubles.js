// .pragma library

function makeFakeGraphVm() {
    return {
        plottableColumns: [1, 2],
        fetchDataCalls: [],
        fetchLatestDataCalls: 0,
        columnName: function (id) { return id === 1 ? "Score" : "Accuracy" },
        columnKey: function (id) { return id === 1 ? "score" : "accuracy" },
        columnColor: function () { return "#00ff00" },
        fetchData: function (scenarioId) { this.fetchDataCalls.push(scenarioId) },
        fetchLatestData: function () { this.fetchLatestDataCalls++ }
    }
}
