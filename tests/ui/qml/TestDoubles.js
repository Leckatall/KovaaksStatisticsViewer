// .pragma library

// The shared draft/enable machinery every settings-VM double needs. Callers pass
// their own fixture rows (allSeriesConfigs) and any extra surface through overrides.
function makeFakeSettingsVm(overrides) {
    return Object.assign({
        allAxes: [],
        pendingChanges: false,
        setSeriesEnabledCalls: 0,
        lastSetSeriesEnabledId: null,
        lastSetSeriesEnabledValue: null,
        setSeriesEnabled: function (id, enabled) {
            this.setSeriesEnabledCalls++
            this.lastSetSeriesEnabledId = id
            this.lastSetSeriesEnabledValue = enabled
        },
        beginDraftCalls: 0,
        beginSeriesDraft: function () { this.beginDraftCalls++ },
        commitDraftCalls: 0,
        commitSeriesDraft: function () { this.commitDraftCalls++; this.pendingChanges = false; return {succeeded: true} },
        discardDraftCalls: 0,
        discardSeriesDraft: function () { this.discardDraftCalls++; this.pendingChanges = false }
    }, overrides)
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
