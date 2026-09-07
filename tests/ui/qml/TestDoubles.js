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

// Benchmark manager VM double: plain data properties drive the dialog's rendering,
// recorder functions capture which commands the dialog forwards, and importResult /
// saveResult / deleteResult script the command outcomes.
function makeFakeBenchmarkManagerVm(overrides) {
    return Object.assign({
        hasDraft: false,
        dirty: false,
        benchmarkName: "",
        draftId: "",
        draftTrackable: false,
        draftFromLibrary: false,
        refreshFailed: false,
        managedDirectoryPath: "C:/Benchmarks",
        libraryEntries: [],
        validationIssues: [],
        root: null,
        tiers: [],
        beginNewCalls: 0,
        beginNewBenchmark: function () { this.beginNewCalls++ },
        openCalls: [],
        openBenchmark: function (id) { this.openCalls.push(id); return true },
        discardCalls: 0,
        discard: function () { this.discardCalls++; this.dirty = false },
        importCalls: [],
        importResult: null,
        importPlaylist: function (url) {
            this.importCalls.push(String(url))
            return this.importResult || {ok: true, skipped: []}
        },
        saveCalls: 0,
        saveResult: null,
        save: function () { this.saveCalls++; return this.saveResult || {ok: true} },
        deleteCalls: [],
        deleteResult: null,
        deleteBenchmark: function (id) {
            this.deleteCalls.push(id)
            return this.deleteResult || {ok: true}
        },
        setBenchmarkNameCalls: [],
        setBenchmarkName: function (name) { this.setBenchmarkNameCalls.push(name) },
        refreshCalls: 0,
        refresh: function () { this.refreshCalls++ },
        addTierCalls: [],
        addTier: function (name) { this.addTierCalls.push(name); return {ok: true, createdId: "tier-1"} },
        renameTierCalls: [],
        renameTier: function (id, name) { this.renameTierCalls.push([id, name]); return {ok: true} },
        setTierColorCalls: [],
        setTierColor: function (id, color) { this.setTierColorCalls.push([id, color]); return {ok: true} },
        reorderTierCalls: [],
        reorderTier: function (id, position) { this.reorderTierCalls.push([id, position]); return {ok: true} },
        removeTierCalls: [],
        removeTier: function (id) { this.removeTierCalls.push(id); return {ok: true} },
        addUnplayedScenarioCalls: [],
        addUnplayedScenario: function (name) {
            this.addUnplayedScenarioCalls.push(name)
            return {ok: true, createdId: "scenario-1"}
        },
        addKnownScenarioCalls: [],
        addKnownScenario: function (name, hash) {
            this.addKnownScenarioCalls.push([name, hash])
            return {ok: true, createdId: "scenario-1"}
        },
        renameScenarioCalls: [],
        renameScenario: function (id, name) { this.renameScenarioCalls.push([id, name]); return {ok: true} },
        removeScenarioCalls: [],
        removeScenario: function (id) { this.removeScenarioCalls.push(id); return {ok: true} },
        setThresholdCalls: [],
        setThreshold: function (entryId, tierId, score) {
            this.setThresholdCalls.push([entryId, tierId, score])
            return {ok: true}
        },
        clearThresholdCalls: [],
        clearThreshold: function (entryId, tierId) {
            this.clearThresholdCalls.push([entryId, tierId])
            return {ok: true}
        },
        addCategoryCalls: [],
        addCategory: function (name) { this.addCategoryCalls.push(name); return {ok: true, createdId: "category-1"} },
        renameGroupCalls: [],
        renameGroup: function (id, name) { this.renameGroupCalls.push([id, name]); return {ok: true} },
        setGroupColorCalls: [],
        setGroupColor: function (id, color) { this.setGroupColorCalls.push([id, color]); return {ok: true} },
        reorderCategoryCalls: [],
        reorderCategory: function (id, position) { this.reorderCategoryCalls.push([id, position]); return {ok: true} },
        removeCategoryCalls: [],
        removeCategory: function (id) { this.removeCategoryCalls.push(id); return {ok: true} },
        addSubcategoryCalls: [],
        addSubcategory: function (categoryId, name) {
            this.addSubcategoryCalls.push([categoryId, name])
            return {ok: true, createdId: "subcategory-1"}
        },
        moveScenarioCalls: [],
        moveScenario: function (entryId, target) {
            this.moveScenarioCalls.push([entryId, target])
            return {ok: true}
        }
    }, overrides)
}
