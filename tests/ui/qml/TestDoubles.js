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

function makeFakeBreakdownNode(overrides) {
    return Object.assign({
        nodeId: "n",
        kind: "scenario",
        name: "Node",
        color: "",
        highestTierText: "",
        childCount: 0,
        nextTierBlocker: false,
        personalBestText: "Unplayed",
        recentAverageText: "No completed runs",
        recentSampleCount: 0,
        attainedTierText: "Unranked",
        nextThresholdText: "Highest tier attained",
        matchStateText: "Resolved",
        expanded: true,
        depth: 0,
        children: []
    }, overrides)
}

function makeFakeBreakdown(overrides) {
    return Object.assign({
        setExpandedCalls: [],
        nodes: [],
        node: function (nodeId) {
            const walk = function (list) {
                for (const n of list) {
                    if (n.nodeId === nodeId)
                        return n
                    const found = walk(n.children || [])
                    if (found)
                        return found
                }
                return null
            }
            return walk(this.nodes)
        },
        setExpanded: function (nodeId, expanded) {
            this.setExpandedCalls.push([nodeId, expanded])
            const n = this.node(nodeId)
            if (n)
                n.expanded = expanded
        }
    }, overrides)
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
        scenarioCount: 0,
        blockers: [],
        rankHistory: makeFakeHistoryModel({metricName: "Personal-best average rank"}),
        playtimeHistory: makeFakeHistoryModel({metricName: "Three-day average playtime"}),
        breakdown: makeFakeBreakdown(),
        selectBenchmarkCalls: [],
        clearSelectionCalls: 0,
        setExpandedCalls: [],
        selectBenchmark: function (id) { this.selectBenchmarkCalls.push(String(id)) },
        clearSelection: function () { this.clearSelectionCalls++ },
        setExpanded: function (nodeId, expanded) {
            this.setExpandedCalls.push([nodeId, expanded])
            this.breakdown.setExpanded(nodeId, expanded)
        }
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
            {id: "", name: "broken.json", classification: "Invalid", selectable: false},
            {id: "", name: "future.json", classification: "Unsupported", selectable: false}
        ]
    }
}

function trackingFixtureNoSelection() {
    return {
        state: "NoSelection",
        stateMessage: "Select a benchmark to begin.",
        selectorEntries: [
            {id: "bench-A", name: "Voltaic S3", classification: "Trackable", selectable: true},
            {id: "", name: "broken.json", classification: "Invalid", selectable: false}
        ]
    }
}

function trackingFixtureUnavailable() {
    return {
        state: "Unavailable",
        stateMessage: "The selected benchmark was deleted or became invalid.",
        selectedName: "Voltaic S3 (last known)",
        selectorEntries: [
            {id: "", name: "broken.json", classification: "Invalid", selectable: false}
        ],
        summaryAvailable: false,
        attainedRank: "",
        completedRank: "",
        nextTier: "",
        averageRank: "",
        totalPlaytime: "",
        blockers: [],
        rankHistory: makeFakeHistoryModel({hasData: false}),
        playtimeHistory: makeFakeHistoryModel({hasData: false}),
        breakdown: makeFakeBreakdown({nodes: []})
    }
}

function trackingFixtureIncomplete() {
    return {
        state: "Incomplete",
        stateMessage: "This benchmark is not ready to rank.",
        selectedName: "Half-built Bench",
        selectorEntries: [
            {id: "bench-I", name: "Half-built Bench", classification: "Incomplete", selectable: true}
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
        blockers: [],
        rankHistory: makeFakeHistoryModel({hasData: false, metricName: "Personal-best average rank"}),
        playtimeHistory: makeFakeHistoryModel({hasData: true, metricName: "Three-day average playtime"}),
        breakdown: makeFakeBreakdown({
            nodes: [
                makeFakeBreakdownNode({
                    nodeId: "sc-1", kind: "scenario", name: "1w4ts",
                    personalBestText: "84.2", recentAverageText: "80.1", recentSampleCount: 3,
                    matchStateText: "Resolved"
                })
            ]
        })
    }
}

function trackingFixtureTrackable() {
    return {
        state: "Trackable",
        stateMessage: "",
        selectedName: "Voltaic S3",
        selectorEntries: [
            {id: "bench-A", name: "Voltaic S3", classification: "Trackable", selectable: true},
            {id: "bench-B", name: "Revo Novice", classification: "Trackable", selectable: true},
            {id: "", name: "broken.json", classification: "Invalid", selectable: false}
        ],
        completenessIssues: [],
        summaryAvailable: true,
        attainedRank: "Gold",
        completedRank: "Silver",
        averageRank: "2.75",
        totalPlaytime: "5 h 40 min",
        nextTier: "Platinum",
        scenariosAtNextTier: 3,
        scenarioCount: 9,
        blockers: [
            {
                kind: "scenario", scenarioName: "1w4ts", groupName: "Clicking / Static",
                name: "1w4ts", currentText: "PB 812", requiredText: "needs 900",
                detail: "PB 812 / needs 900"
            },
            {
                kind: "scenario", scenarioName: "popcorn", groupName: "Tracking / Smoothness",
                name: "popcorn", currentText: "Unplayed", requiredText: "needs 78.0",
                detail: "Unplayed / needs 78.0"
            },
            {
                kind: "group", groupName: "Switching", name: "Switching",
                currentText: "1 of 3 scenarios at Platinum", requiredText: "needs 3",
                detail: "Group requirement unmet"
            }
        ],
        rankHistory: makeFakeHistoryModel({
            metricName: "Personal-best average rank", hasData: true,
            emptyStateText: "No ranked history yet"
        }),
        playtimeHistory: makeFakeHistoryModel({
            metricName: "Three-day average playtime", hasData: true,
            emptyStateText: "No playtime history yet"
        }),
        breakdown: makeFakeBreakdown({
            nodes: [
                makeFakeBreakdownNode({
                    nodeId: "cat-click", kind: "category", name: "Clicking", color: "#00A000",
                    highestTierText: "Gold", childCount: 2, nextTierBlocker: true, expanded: true,
                    children: [
                        makeFakeBreakdownNode({
                            nodeId: "sub-static", kind: "subcategory", name: "Static",
                            color: "#00A000", highestTierText: "Gold", childCount: 1,
                            nextTierBlocker: true, expanded: true,
                            children: [
                                makeFakeBreakdownNode({
                                    nodeId: "sc-1w4ts", kind: "scenario", name: "1w4ts",
                                    personalBestText: "812", recentAverageText: "790.5",
                                    recentSampleCount: 0, attainedTierText: "Gold",
                                    nextThresholdText: "Platinum 900", matchStateText: "Resolved",
                                    nextTierBlocker: true, depth: 2
                                })
                            ]
                        })
                    ]
                }),
                makeFakeBreakdownNode({
                    nodeId: "grp-uncat", kind: "uncategorized", name: "Uncategorized",
                    color: "", highestTierText: "Silver", childCount: 1, expanded: true,
                    children: [
                        makeFakeBreakdownNode({
                            nodeId: "sc-popcorn", kind: "scenario", name: "popcorn",
                            personalBestText: "Unplayed", recentAverageText: "No completed runs",
                            recentSampleCount: 0, attainedTierText: "Unranked",
                            nextThresholdText: "Bronze 60", matchStateText: "Ambiguous",
                            nextTierBlocker: true, depth: 1
                        })
                    ]
                })
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
        {id: "g1", kind: "category", parentId: "", name: "Clicking", color: "#009600", scenarioCount: 2, issues: []},
        {id: "g2", kind: "category", parentId: "", name: "Tracking", color: "#000096", scenarioCount: 1, issues: []},
        {id: "p", kind: "subcategory", parentId: "g2", name: "Precise", color: "#960000", scenarioCount: 1, issues: []},
        {id: "hollow", kind: "subcategory", parentId: "g2", name: "Hollow", color: "#444444", scenarioCount: 0,
         issues: ["A subcategory has no scenarios."]},
        {id: "gE", kind: "category", parentId: "", name: "Empty", color: "#888888", scenarioCount: 0,
         issues: ["A category has neither scenarios nor subcategories."]}
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
