// .pragma library

function makeFakeGraphVm() {
    return {
        allColumns: [1, 2],
        enabledColumns: [1, 2],
        allSeries: [
            {id: "1", name: "Score", color: "#00ff00", column: 1},
            {id: "2", name: "Accuracy", color: "#00ff00", column: 2}
        ],
        enabledSeriesIds: ["1", "2"],
        fetchDataCalls: [],
        fetchLatestDataCalls: 0,
        columnYAxis: function (id) { return id },
        fetchData: function (scenarioId) { this.fetchDataCalls.push(scenarioId) },
        fetchLatestData: function () { this.fetchLatestDataCalls++ }
    }
}

function makeFakeHistoryVm() {
    return {
        scenarioTitle: "Air Angelic",
        runCount: 0,
        allSeries: [
            {id: "1", name: "Score", column: 1},
            {id: "2", name: "Accuracy", column: 2},
            {id: "3", name: "Shots", column: 3},
            {id: "4", name: "Hits", column: 4},
            {id: "5", name: "Misses", column: 5}
        ]
    }
}
