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
        setScenarioHashCalls: [],
        setScenarioHash: function (entryId, hash) {
            this.setScenarioHashCalls.push([entryId, hash])
            return {ok: true}
        },
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
