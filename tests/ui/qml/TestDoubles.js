.pragma library

function makeFakeGraphVm() {
    return {
        plottableColumns: [1, 2],
        fetchDataCalls: [],
        columnName: function (id) { return id === 1 ? "Score" : "Accuracy" },
        columnColor: function () { return "#00ff00" },
        fetchData: function (scenarioId) { this.fetchDataCalls.push(scenarioId) }
    }
}
