// .pragma library

// The shared visit/enable machinery every settings-VM double needs. Callers pass
// their own fixture rows (allSeriesConfigs) and any extra surface through overrides.
// `saveResult` decides whether save() is accepted; an accepted save leaves the visit clean.
function makeFakeSettingsVm(overrides) {
    return Object.assign({
        allAxes: [],
        pendingChanges: false,
        issues: [],
        runAxisLabelSeriesId: "",
        setRunAxisLabelSeriesIdCalls: [],
        setRunAxisLabelSeriesId: function (id) { this.setRunAxisLabelSeriesIdCalls.push(id) },
        setSeriesEnabledCalls: 0,
        lastSetSeriesEnabledId: null,
        lastSetSeriesEnabledValue: null,
        setSeriesEnabled: function (id, enabled) {
            this.setSeriesEnabledCalls++
            this.lastSetSeriesEnabledId = id
            this.lastSetSeriesEnabledValue = enabled
        },
        beginVisitCalls: 0,
        beginVisit: function () { this.beginVisitCalls++ },
        saveCalls: 0,
        saveResult: {accepted: true, issues: []},
        save: function () {
            this.saveCalls++
            if (this.saveResult.accepted) this.pendingChanges = false
            return this.saveResult
        },
        revertCalls: 0,
        revertChanges: function () { this.revertCalls++; this.pendingChanges = false },
        endVisitCalls: 0,
        endVisit: function () { this.endVisitCalls++; this.pendingChanges = false }
    }, overrides)
}

function makeFakeHistoryVm() {
    return {
        scenarioTitle: "Air Angelic",
        runCount: 0,
        visibleRunCount: 0,
        selectedRunIndex: -1,
        selectedRunVisible: false,
        allSeries: [
            {id: "1", name: "Score", column: 1},
            {id: "2", name: "Accuracy", column: 2},
            {id: "3", name: "Shots", column: 3},
            {id: "4", name: "Hits", column: 4},
            {id: "5", name: "Misses", column: 5}
        ]
    }
}

// ---------------------------------------------------------------------------
// Benchmark tracking workspace doubles (plan 03 / W1-W3).
//
// `state` tokens the workspace body switches on:
//   "EmptyLibrary"  - no benchmark files installed at all
//   "ProblemsOnly"  - only Invalid/Unsupported files present, none selectable
//   "NoSelection"   - selectable entries exist, none chosen
//   "Unavailable"   - a selected id whose definition vanished/became a problem
//   "Incomplete"    - Ready + Completeness::Incomplete
//   "Trackable"     - Ready + complete
// ---------------------------------------------------------------------------

function makeFakeHistoryModel(overrides) {
    return Object.assign({
        metricName: "Personal-best average rank",
        hasData: false,
        emptyStateText: "No history yet",
        // Minimal GraphViewModelBase-ish surface so a card can bind it as graphVm
        // without throwing while the metric is empty.
        contentState: 0,
        enabledSeriesIds: [],
        allSeries: [],
        series: function () { return [] },
        columnForSeriesId: function () { return -1 },
        seriesIdForColumn: function () { return "" }
    }, overrides)
}

// BenchmarkProgressGridModel double: the tier ladder, one map per scenario row and
// the group cells spanning them.
function makeFakeProgressGrid(overrides) {
    return Object.assign({
        tiers: [],
        rows: [],
        groups: [],
        hasSubcategories: false
    }, overrides)
}

function makeFakeProgressRow(overrides) {
    return Object.assign({
        entryId: "n",
        name: "Scenario",
        personalBestText: "—",
        personalBestDateText: "—",
        playCountText: "—",
        attainedTierText: "Unranked",
        attainedTierColor: "",
        fills: [],
        nextTierText: "",
        nextTierColor: "",
        nextThresholdText: "—",
        matchStateText: "",
        matchStateHint: "",
        barDescription: "Unplayed",
        blocker: false,
        blockerColor: "",
        blockerText: ""
    }, overrides)
}

function fakeProgressTiers() {
    return [
        {id: "bronze", name: "Bronze", color: "#cd7f32", barColor: "#cd7f32"},
        {id: "silver", name: "Silver", color: "#c0c0c8", barColor: "#c0c0c8"},
        {id: "gold", name: "Gold", color: "#ffc828", barColor: "#ffc828"},
        {id: "platinum", name: "Platinum", color: "#50dcdc", barColor: "#50dcdc"}
    ]
}

function makeFakeTrackingVm(overrides) {
    const vm = Object.assign({
        state: "NoSelection",
        stateMessage: "Select a benchmark to begin.",
        selectedName: "",
        selectorEntries: [],
        completenessIssues: [],
        summaryAvailable: false,
        attainedRank: "Unranked",
        completedRank: "None completed",
        nextTier: "Highest tier attained",
        averageRank: "0",
        totalPlaytime: "<1 min",
        scenariosAtNextTier: 0,
        nextTierBlockerCount: 0,
        hasNextTier: false,
        scenarioCount: 0,
        rankHistory: makeFakeHistoryModel({metricName: "Personal-best average rank"}),
        playtimeHistory: makeFakeHistoryModel({metricName: "Three-day average playtime"}),
        progressGrid: makeFakeProgressGrid(),
        selectBenchmarkCalls: [],
        clearSelectionCalls: 0,
        selectBenchmark: function (id) { this.selectBenchmarkCalls.push(String(id)) },
        clearSelection: function () { this.clearSelectionCalls++ },
        // The real view model asks the use case to restore the remembered (or only)
        // benchmark; the double just records the request.
        selectDefaultBenchmarkCalls: 0,
        selectDefaultBenchmark: function () { this.selectDefaultBenchmarkCalls++ }
    }, overrides)
    return vm
}

// Fixtures for the six workspace bodies. Each returns an overrides object for
// makeFakeTrackingVm.

function trackingFixtureEmptyLibrary() {
    return {
        state: "EmptyLibrary",
        stateMessage: "No benchmarks are installed.",
        selectorEntries: []
    }
}

function trackingFixtureProblemsOnly() {
    return {
        state: "ProblemsOnly",
        stateMessage: "Some benchmark files need attention.",
        selectorEntries: [
            {id: "", name: "broken.json", filename: "broken.json", classification: "Invalid", selectable: false},
            {id: "", name: "future.json", filename: "future.json", classification: "Unsupported", selectable: false}
        ]
    }
}

function trackingFixtureNoSelection() {
    return {
        state: "NoSelection",
        stateMessage: "Select a benchmark to begin.",
        selectorEntries: [
            {id: "bench-A", name: "Voltaic S3", filename: "voltaic-s3.json", classification: "Trackable", selectable: true},
            {id: "", name: "broken.json", filename: "broken.json", classification: "Invalid", selectable: false}
        ]
    }
}

function trackingFixtureUnavailable() {
    return {
        state: "Unavailable",
        stateMessage: "The selected benchmark was deleted or became invalid.",
        selectedName: "Voltaic S3 (last known)",
        selectorEntries: [
            {id: "", name: "broken.json", filename: "broken.json", classification: "Invalid", selectable: false}
        ],
        summaryAvailable: false,
        attainedRank: "",
        completedRank: "",
        nextTier: "",
        averageRank: "",
        totalPlaytime: "",
        rankHistory: makeFakeHistoryModel({hasData: false}),
        playtimeHistory: makeFakeHistoryModel({hasData: false}),
        progressGrid: makeFakeProgressGrid()
    }
}

function trackingFixtureIncomplete() {
    return {
        state: "Incomplete",
        stateMessage: "This benchmark is not ready to rank.",
        selectedName: "Half-built Bench",
        selectorEntries: [
            {id: "bench-I", name: "Half-built Bench", filename: "half-built.json", classification: "Incomplete", selectable: true}
        ],
        completenessIssues: [
            "Tier \"Gold\" has no threshold for scenario \"1w4ts\".",
            "Scenario \"popcorn\" is not placed in any category."
        ],
        summaryAvailable: false,
        attainedRank: "",
        completedRank: "",
        nextTier: "",
        averageRank: "",
        totalPlaytime: "3 h 12 min",
        scenariosAtNextTier: 0,
        scenarioCount: 4,
        rankHistory: makeFakeHistoryModel({hasData: false, metricName: "Personal-best average rank"}),
        playtimeHistory: makeFakeHistoryModel({hasData: true, metricName: "Three-day average playtime"}),
        progressGrid: makeFakeProgressGrid({
            tiers: fakeProgressTiers(),
            rows: [
                makeFakeProgressRow({
                    entryId: "sc-1", name: "1w4ts", personalBestText: "84.2", personalBestDateText: "2026-07-30", playCountText: "3",
                    fills: [1, 0.4, 0, 0], barDescription: "Bronze · 40% of the way to Silver (84.2 / 90)"
                })
            ],
            groups: [{column: "category", id: "", name: "Uncategorized", color: "", start: 0, span: 1, spansSubcategory: true}]
        })
    }
}

function trackingFixtureTrackable() {
    return {
        state: "Trackable",
        stateMessage: "",
        selectedName: "Voltaic S3",
        selectorEntries: [
            {id: "bench-A", name: "Voltaic S3", filename: "voltaic-s3.json", classification: "Trackable", selectable: true},
            {id: "bench-B", name: "Revo Novice", filename: "revo-novice.json", classification: "Trackable", selectable: true},
            {id: "", name: "broken.json", filename: "broken.json", classification: "Invalid", selectable: false}
        ],
        completenessIssues: [],
        summaryAvailable: true,
        attainedRank: "Gold",
        completedRank: "Silver",
        averageRank: "Silver · 75% to Gold",
        totalPlaytime: "5 h 40 min",
        nextTier: "Platinum",
        attainedRankColor: "#ffc828",
        completedRankColor: "#c0c0c8",
        nextTierColor: "#50dcdc",
        scenariosAtNextTier: 3,
        nextTierBlockerCount: 2,
        hasNextTier: true,
        scenarioCount: 9,
        rankHistory: makeFakeHistoryModel({
            metricName: "Personal-best average rank", hasData: true,
            emptyStateText: "No ranked history yet"
        }),
        playtimeHistory: makeFakeHistoryModel({
            metricName: "Three-day average playtime", hasData: true,
            emptyStateText: "No playtime history yet"
        }),
        // Rows in definition order: Uncategorized first, then Clicking's two
        // subcategories, then Tracking's direct scenario.
        progressGrid: makeFakeProgressGrid({
            tiers: fakeProgressTiers(),
            hasSubcategories: true,
            rows: [
                makeFakeProgressRow({
                    entryId: "sc-popcorn", name: "popcorn", fills: [0, 0, 0, 0],
                    nextTierText: "Bronze", nextTierColor: "#cd7f32", nextThresholdText: "60",
                    matchStateText: "Several scenarios match",
                    matchStateHint: "More than one scenario in your runs has this name.",
                    barDescription: "Unplayed\nSeveral scenarios match: More than one scenario in your runs has this name.",
                    blocker: true, blockerColor: "#50dcdc", blockerText: "Blocks Platinum (needs 78)"
                }),
                makeFakeProgressRow({
                    entryId: "sc-1w4ts", name: "1w4ts", personalBestText: "812", personalBestDateText: "2026-08-14", playCountText: "37",
                    attainedTierText: "Gold", attainedTierColor: "#ffc828", fills: [1, 1, 1, 0.75],
                    nextTierText: "Platinum", nextTierColor: "#50dcdc", nextThresholdText: "900",
                    barDescription: "Gold · 75% of the way to Platinum (812 / 900)",
                    blocker: true, blockerColor: "#50dcdc", blockerText: "Blocks Platinum (needs 900)"
                }),
                makeFakeProgressRow({
                    entryId: "sc-pasu", name: "Pasu Reload", personalBestText: "1,040.25", personalBestDateText: "2026-08-02",
                    playCountText: "2",
                    attainedTierText: "Platinum", attainedTierColor: "#50dcdc", fills: [1, 1, 1, 1],
                    nextThresholdText: "Highest tier attained",
                    barDescription: "Platinum · top tier reached (1,040.25 / 1,000)"
                }),
                makeFakeProgressRow({
                    entryId: "sc-smooth", name: "Smoothbot", personalBestText: "3,120", personalBestDateText: "2026-08-21", playCountText: "1,204",
                    attainedTierText: "Silver", attainedTierColor: "#c0c0c8", fills: [1, 1, 0.2, 0],
                    nextTierText: "Gold", nextTierColor: "#ffc828", nextThresholdText: "3,400",
                    barDescription: "Silver · 20% of the way to Gold (3,120 / 3,400)"
                })
            ],
            groups: [
                {column: "category", id: "", name: "Uncategorized", color: "", start: 0, span: 1, spansSubcategory: true},
                {column: "subcategory", id: "sub-static", name: "Static", color: "#00A000", start: 1, span: 1, spansSubcategory: false},
                {column: "subcategory", id: "sub-dynamic", name: "Dynamic", color: "#007000", start: 2, span: 1, spansSubcategory: false},
                {column: "category", id: "cat-click", name: "Clicking", color: "#00A000", start: 1, span: 2, spansSubcategory: false},
                {column: "category", id: "cat-track", name: "Tracking", color: "#9c27b0", start: 3, span: 1, spansSubcategory: true}
            ]
        })
    }
}

// Benchmark manager VM double: plain data properties drive the dialog's rendering,
// recorder functions capture which commands the dialog forwards, and importResult /
// saveResult / deleteResult script the command outcomes.
function makeFakeBenchmarkManagerVm(overrides) {
    return Object.assign({
        hasDraft: false,
        dirty: false,
        baselineStale: false,
        benchmarkName: "",
        draftId: "",
        draftTrackable: false,
        draftFromLibrary: false,
        refreshFailed: false,
        resolutionWriteFailed: false,
        managedDirectoryPath: "C:/Benchmarks",
        libraryEntries: [],
        validationIssues: [],
        scenarioCatalogue: [],
        tiers: [],
        beginNewCalls: 0,
        beginNewBenchmark: function () { this.beginNewCalls++ },
        openCalls: [],
        openBenchmark: function (id) { this.openCalls.push(id); return true },
        discardCalls: 0,
        discard: function () { this.discardCalls++; this.dirty = false; this.hasDraft = false },
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
        addTier: function (name) { this.commandLog.push("addTier"); this.addTierCalls.push(name); return {ok: true, createdId: "tier-1"} },
        renameTierCalls: [],
        renameTier: function (id, name) { this.renameTierCalls.push([id, name]); return {ok: true} },
        setTierColorCalls: [],
        setTierColor: function (id, color) { this.setTierColorCalls.push([id, color]); return {ok: true} },
        reorderTierCalls: [],
        reorderTier: function (id, position) { this.commandLog.push("reorderTier"); this.reorderTierCalls.push([id, position]); return {ok: true} },
        removeTierCalls: [],
        removeTier: function (id) { this.commandLog.push("removeTier"); this.removeTierCalls.push(id); return {ok: true} },
        addUnplayedScenarioCalls: [],
        addUnplayedScenario: function (name) {
            this.commandLog.push("addUnplayedScenario")
            this.addUnplayedScenarioCalls.push(name)
            return {ok: true, createdId: "scenario-1"}
        },
        addKnownScenarioCalls: [],
        addKnownScenario: function (name, hash) {
            this.commandLog.push("addKnownScenario")
            this.addKnownScenarioCalls.push([name, hash])
            return {ok: true, createdId: "scenario-1"}
        },
        renameScenarioCalls: [],
        renameScenario: function (id, name) { this.renameScenarioCalls.push([id, name]); return {ok: true} },
        setScenarioIdentityCalls: [],
        setScenarioIdentity: function (id, name, hash) {
            this.commandLog.push("setScenarioIdentity")
            this.setScenarioIdentityCalls.push([id, name, hash])
            return {ok: true}
        },
        setScenarioHashesCalls: [],
        setScenarioHashes: function (entryId, hashes) {
            this.setScenarioHashesCalls.push([entryId, Array.prototype.slice.call(hashes)])
            return {ok: true}
        },
        setScenarioHashCalls: [],
        setScenarioHash: function (entryId, hash) {
            this.setScenarioHashCalls.push([entryId, hash])
            return {ok: true}
        },
        removeScenarioCalls: [],
        removeScenario: function (id) { this.commandLog.push("removeScenario"); this.removeScenarioCalls.push(id); return {ok: true} },
        removeScenariosCalls: [],
        removeScenarios: function (ids) {
            this.commandLog.push("removeScenarios")
            this.removeScenariosCalls.push(Array.prototype.slice.call(ids))
            return {ok: true}
        },
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
        addCategory: function (name) { this.commandLog.push("addCategory"); this.addCategoryCalls.push(name); return {ok: true, createdId: "category-1"} },
        renameGroupCalls: [],
        renameGroup: function (id, name) { this.renameGroupCalls.push([id, name]); return {ok: true} },
        setGroupColorCalls: [],
        setGroupColor: function (id, color) { this.setGroupColorCalls.push([id, color]); return {ok: true} },
        reorderCategoryCalls: [],
        reorderCategory: function (id, position) { this.commandLog.push("reorderCategory"); this.reorderCategoryCalls.push([id, position]); return {ok: true} },
        removeCategoryCalls: [],
        removeCategory: function (id) { this.commandLog.push("removeCategory"); this.removeCategoryCalls.push(id); return {ok: true} },
        addSubcategoryCalls: [],
        addSubcategory: function (categoryId, name) {
            this.addSubcategoryCalls.push([categoryId, name])
            return {ok: true, createdId: "subcategory-1"}
        },
        moveScenarioCalls: [],
        moveScenario: function (entryId, target) {
            this.moveScenarioCalls.push([entryId, target])
            return {ok: true}
        },
        // ---- Table editor surface. `tableModel` is supplied by tests that render rows, from a
        // BenchmarkTableModelFixture; commandLog records every table-era command in call order.
        tableModel: null,
        groups: [],
        assignTargets: [{id: "", label: "Uncategorized"}],
        selectedEntryIds: [],
        currentCell: ({}),
        canUndo: false,
        normalizationIssues: [],
        commandLog: [],
        editThresholdTextCalls: [],
        editThresholdText: function (entryId, tierId, text) {
            this.commandLog.push("editThresholdText")
            this.editThresholdTextCalls.push([entryId, tierId, text])
            return {ok: true}
        },
        pasteTextCalls: [],
        pasteResult: null,
        pasteText: function (destination, text) {
            this.commandLog.push("pasteText")
            this.pasteTextCalls.push([destination, text])
            return this.pasteResult || {ok: true}
        },
        undoCalls: 0,
        undo: function () { this.commandLog.push("undo"); this.undoCalls++; return {ok: true} },
        assignScenariosCalls: [],
        assignResult: null,
        assignScenarios: function (entryIds, target, before) {
            this.commandLog.push("assignScenarios")
            this.assignScenariosCalls.push(before ? [entryIds, target, before] : [entryIds, target])
            return this.assignResult || {ok: true}
        },
        addSubcategoryRelocatingCalls: [],
        addSubcategoryRelocating: function (categoryId, name, relocation) {
            this.commandLog.push("addSubcategoryRelocating")
            this.addSubcategoryRelocatingCalls.push([categoryId, name, relocation])
            return {ok: true, createdId: "subcategory-1"}
        },
        reorderSubcategoryCalls: [],
        reorderSubcategory: function (id, position) {
            this.commandLog.push("reorderSubcategory")
            this.reorderSubcategoryCalls.push([id, position])
            return {ok: true}
        },
        reorderScenarioCalls: [],
        reorderScenario: function (id, position) {
            this.commandLog.push("reorderScenario")
            this.reorderScenarioCalls.push([id, position])
            return {ok: true}
        }
    }, overrides)
}

// A dialog fixture: two direct rows in g1, a subcategory p (row c) in g2, an ambiguous-mapped row
// in Uncategorized, an empty category and an empty subcategory, and a ladder whose second rank is
// unnamed.
function benchmarkManagementDesc() {
    const g1 = {id: "g1", name: "Clicking", color: "#009600"}
    const g2 = {id: "g2", name: "Tracking", color: "#000096"}
    const p = {id: "p", name: "Precise", color: "#960000"}
    return {
        tiers: [{id: "t1", name: "Gold", color: "#FFD700"}, {id: "t2", name: ""}],
        rows: [
            {entryId: "a", name: "1w6ts", category: g1, cells: {t1: {displayText: "100"}}},
            {entryId: "b", name: "1w4ts", category: g1, cells: {}},
            {entryId: "c", name: "smoothbot", category: g2, subcategory: p, cells: {}},
            {entryId: "u-res", name: "resolved one", mappingState: "resolved",
             mappingCandidates: [benchmarkManagerCandidate("hash-a", 5, new Date(2024, 2, 3))], cells: {}},
            {entryId: "u-amb", name: "ambiguous one", mappingState: "ambiguous",
             mappingCandidates: [benchmarkManagerCandidate("hash-a", 3, new Date(2023, 10, 14)),
                                 benchmarkManagerCandidate("hash-b", 7, new Date(2024, 0, 2))],
             cells: {}}
        ]
    }
}

function benchmarkManagementGroups() {
    return [
        {id: "g1", kind: "category", parentId: "", name: "Clicking", color: "#009600", scenarioCount: 2, acceptsScenarios: true, subcategoryNeedsRelocation: true, issues: []},
        {id: "g2", kind: "category", parentId: "", name: "Tracking", color: "#000096", scenarioCount: 1, acceptsScenarios: false, subcategoryNeedsRelocation: false, issues: []},
        {id: "p", kind: "subcategory", parentId: "g2", name: "Precise", color: "#960000", scenarioCount: 1, acceptsScenarios: true, issues: []},
        {id: "hollow", kind: "subcategory", parentId: "g2", name: "Hollow", color: "#444444", scenarioCount: 0, acceptsScenarios: true,
         issues: ["A subcategory has no scenarios."]},
        {id: "gE", kind: "category", parentId: "", name: "Empty", color: "#888888", scenarioCount: 0, acceptsScenarios: true, subcategoryNeedsRelocation: false,
         issues: ["A category has neither scenarios nor subcategories."]}
    ]
}

function benchmarkManagementAssignTargets() {
    return [
        {id: "", label: "Uncategorized"},
        {id: "g1", label: "Clicking"},
        {id: "p", label: "Tracking / Precise"},
        {id: "hollow", label: "Tracking / Hollow"},
        {id: "gE", label: "Empty"}
    ]
}

// ---------------------------------------------------------------------------
// Manager scenario-resolution fixtures (plan 04 / M2).
// ---------------------------------------------------------------------------

// Two entries share the display name "1w4ts"; only the hash tells them apart, so
// the known-scenario picker must key its selection on hash, not name.
function benchmarkManagerScenarioCatalogue() {
    return [
        {name: "1w4ts", hash: "hash-a"},
        {name: "1w4ts", hash: "hash-b"},
        {name: "popcorn", hash: "hash-c"},
        {name: "Air Angelic 4", hash: "hash-d"}
    ]
}

function benchmarkManagerCandidate(hash, runCount, lastPlayed) {
    return {hash: hash, runCount: runCount, lastPlayed: lastPlayed}
}

// One scenario per matching state so a test can assert every textual label. The
// ambiguous entry carries the candidate metadata (hash / run count / last-played)
// the dialog must show before the user picks one.
function benchmarkManagerResolutionTree() {
    return {
        nodeId: "", kind: "uncategorized", name: "Uncategorized",
        children: [
            {nodeId: "s-res", kind: "scenario", name: "resolved one", hasHash: true,
             matchState: "resolved",
             candidates: [
                 benchmarkManagerCandidate("hash-a", 5, new Date(2024, 2, 3)),
                 benchmarkManagerCandidate("hash-x", 1, new Date(2023, 6, 20))
             ],
             thresholds: [{tierId: "t1", tierName: "Gold", score: 90, hasValue: true}]},
            {nodeId: "s-map", kind: "scenario", name: "mapped gone", hasHash: true,
             matchState: "mappedUnavailable",
             candidates: [benchmarkManagerCandidate("hash-a", 5, new Date(2024, 2, 3))],
             thresholds: [{tierId: "t1", tierName: "Gold", score: 80, hasValue: true}]},
            {nodeId: "s-unr", kind: "scenario", name: "unresolved one", hasHash: false,
             matchState: "unresolved", candidates: [], thresholds: []},
            {nodeId: "s-amb", kind: "scenario", name: "ambiguous one", hasHash: false,
             matchState: "ambiguous",
             candidates: [
                 benchmarkManagerCandidate("hash-a", 3, new Date(2023, 10, 14)),
                 benchmarkManagerCandidate("hash-b", 7, new Date(2024, 0, 2))
             ],
             thresholds: []},
            {nodeId: "s-auto", kind: "scenario", name: "auto one", hasHash: false,
             matchState: "autoMappable",
             candidates: [benchmarkManagerCandidate("hash-d", 12, new Date(2024, 5, 1))],
             thresholds: []}
        ]
    }
}

// ---------------------------------------------------------------------------
// Benchmark table editor fixtures.
// ---------------------------------------------------------------------------

// Builds the BenchmarkTableModelFixture projection from a compact description. Each row names its
// category / subcategory ({id, name, color}, or null for Uncategorized / none) and its cells keyed
// by tier id; spans are derived from consecutive rows sharing a group id, as the view model does.
function benchmarkTableProjection(desc) {
    const tiers = desc.tiers.map(function (tier) {
        return {id: tier.id, name: tier.name, color: tier.color || "#808080", issues: tier.issues || []}
    })
    const rows = desc.rows.map(function (row, index) {
        const cells = desc.tiers.map(function (tier) {
            const cell = (row.cells || {})[tier.id] || {}
            return {
                displayText: cell.displayText !== undefined ? cell.displayText : "",
                editText: cell.editText !== undefined ? cell.editText
                                                      : (cell.displayText !== undefined ? cell.displayText : ""),
                hasValue: cell.hasValue !== undefined ? cell.hasValue : cell.displayText !== undefined && !cell.inputState,
                inputState: cell.inputState || "",
                issues: cell.issues || []
            }
        })
        return {
            entryId: row.entryId, name: row.name !== undefined ? row.name : "Scenario " + row.entryId,
            scenarioIssues: row.scenarioIssues || [],
            category: row.category ? {id: row.category.id, name: row.category.name,
                                      color: row.category.color || "#336699", issues: row.category.issues || []}
                                   : {id: "", name: "Uncategorized", issues: []},
            subcategory: row.subcategory ? {id: row.subcategory.id, name: row.subcategory.name,
                                            color: row.subcategory.color || "#669933",
                                            issues: row.subcategory.issues || []}
                                         : {id: "", name: "", issues: []},
            mappingState: row.mappingState || "unresolved",
            mappingCandidates: row.mappingCandidates || [],
            mappedHashes: row.mappedHashes || [],
            cells: cells
        }
    })
    const assignSpans = function (key) {
        let start = 0
        for (let i = 0; i <= rows.length; ++i) {
            if (i < rows.length && i > start && rows[i][key].id === rows[start][key].id
                    && (key !== "subcategory" || rows[i][key].id !== "")) continue
            for (let j = start; j < i; ++j) {
                rows[j][key].spanStart = start
                rows[j][key].spanLength = key === "subcategory" && rows[j][key].id === "" ? 0 : i - start
            }
            start = i
        }
    }
    assignSpans("category")
    assignSpans("subcategory")
    return {tiers: tiers, rows: rows}
}

// `count` scenario rows: the first `firstSpan` in category g1, the rest in g2, three tiers, and an
// invalid retained input at `invalidRow` / t2.
function benchmarkTableLargeDesc(count, firstSpan, invalidRow) {
    const rows = []
    for (let i = 0; i < count; ++i) {
        const cells = {t1: {displayText: String(i)}, t3: {displayText: String(i + 2)}}
        cells.t2 = i === invalidRow ? {displayText: "12oops", inputState: "invalid",
                                        issues: ["Not a number. Saving stores this threshold as missing."]}
                                    : {displayText: String(i + 1)}
        rows.push({entryId: "e" + i,
                   category: i < firstSpan ? {id: "g1", name: "Category g1"} : {id: "g2", name: "Category g2"},
                   cells: cells})
    }
    return {tiers: [{id: "t1", name: "Bronze"}, {id: "t2", name: "Silver"}, {id: "t3", name: "Gold"}], rows: rows}
}

// A manager double for BenchmarkEditorTable: `tableModel` is a real model fed by the fixture, and
// every table command is recorded in call order in `commandLog` as well as its own list.
function makeFakeTableManager(fixture, overrides) {
    const record = function (vm, name, args) {
        vm.commandLog.push(name)
        vm[name + "Calls"].push(args)
    }
    return Object.assign({
        tableModel: fixture.model,
        groups: [],
        tiers: [],
        dirty: false,
        draftFromLibrary: false,
        selectedEntryIds: [],
        currentCell: ({}),
        canUndo: false,
        commandLog: [],
        editThresholdTextCalls: [],
        editThresholdText: function (entryId, tierId, text) {
            record(this, "editThresholdText", [entryId, tierId, text]); return {ok: true}
        },
        renameScenarioCalls: [],
        renameScenario: function (id, name) { record(this, "renameScenario", [id, name]); return {ok: true} },
        renameGroupCalls: [],
        renameGroup: function (id, name) { record(this, "renameGroup", [id, name]); return {ok: true} },
        renameTierCalls: [],
        renameTier: function (id, name) { record(this, "renameTier", [id, name]); return {ok: true} },
        pasteTextCalls: [],
        pasteResult: null,
        pasteText: function (destination, text) {
            record(this, "pasteText", [destination, text]); return this.pasteResult || {ok: true}
        },
        undoCalls: [],
        undo: function () { record(this, "undo", []); return {ok: true} }
    }, overrides || {})
}

// The resolution tree's five matching states as table rows (all in Uncategorized, one tier).
function benchmarkResolutionDesc() {
    const tree = benchmarkManagerResolutionTree()
    return {
        tiers: [{id: "t1", name: "Gold", color: "#FFD700"}],
        rows: tree.children.map(function (node) {
            const threshold = (node.thresholds || []).find(function (t) { return t.tierId === "t1" })
            return {entryId: node.nodeId, name: node.name, mappingState: node.matchState,
                    mappingCandidates: node.candidates || [],
                    cells: threshold ? {t1: {displayText: String(threshold.score)}} : {}}
        })
    }
}
