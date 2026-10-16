import QtQuick

// The slice of SettingsViewModel that the graph gear popups and the Graphs settings page use, with
// notifying properties so bindings follow changes the way the real one does.
// Count text is validated like the real view model: a whole number from 1 to the field's limit.
// The draft side keeps what the page was told (`calls`) and reflects it in `graphDrafts` without parsing;
// a test that wants an error sets `draftIssues` itself.
QtObject {
    id: vm

    property string selectedGraph: "run"

    // What the Settings window needs besides the graph surface.
    property url kovaaksDir: "file:///C:/Kovaaks"
    property url profilePath: "file:///C:/Profile/profile.pb"
    property bool profileLoaded: false
    property var allSeriesConfigs: []
    property var allAxes: []
    property var rawInputs: []
    property string runAxisLabelSeriesId: ""
    property bool pendingChanges: false
    property int beginVisitCalls: 0
    property int endVisitCalls: 0
    function beginVisit() { beginVisitCalls++ }
    function endVisit() { endVisitCalls++; pendingChanges = false }
    function save() {
        saveCalls++
        if (saveResult.accepted) pendingChanges = false
        else issues = saveResult.issues
        return saveResult
    }
    function revertChanges() { pendingChanges = false }
    function setRunAxisLabelSeriesId(id) {}
    function setKovaaksDir(dir) {}
    function setProfilePath(path) {}
    property var patches: []
    property var issues: []
    property var saveResult: ({accepted: true, issues: []})
    property int saveCalls: 0
    // Non-empty: the next applyQuickPatch is rejected with these issues and changes nothing.
    property var rejectWith: []

    property var quickSettings: ({
        run: entry({hasRange: false, hasLatestRuns: false, hasWindow: false, markers: false}),
        dailyPlaytime: entry({hasWindow: true, window: 3}),
        scenarioHistory: entry({hasLatestRuns: true, markers: true}),
        benchmarkRank: entry({}),
        benchmarkPlaytime: entry({hasWindow: true, window: 3})
    })

    // The real view model hands QML every number and date already formatted for the locale; the text
    // keys follow their value (plain digits, ISO dates) unless a test names them.
    function withTexts(merged, overrides) {
        const derived = {
            rangeCountText: () => String(merged.rangeCount),
            windowText: () => String(merged.window),
            rangeStartFormatted: () => merged.rangeStart,
            rangeEndFormatted: () => merged.rangeEnd
        }
        const sources = {rangeCountText: "rangeCount", windowText: "window",
                         rangeStartFormatted: "rangeStart", rangeEndFormatted: "rangeEnd"}
        for (const key of Object.keys(derived))
            if (overrides[key] === undefined && (overrides[sources[key]] !== undefined || merged[key] === undefined))
                merged[key] = derived[key]()
        return merged
    }

    function entry(overrides) {
        return withTexts(Object.assign({
            hasRange: true,
            hasLatestRuns: false,
            hasWindow: false,
            rangeKind: "allHistory",
            rangeCount: 0,
            rangeStart: "",
            rangeEnd: "",
            window: 0,
            connection: "smooth",
            markers: false
        }, overrides), overrides)
    }

    function setEntry(graph, overrides) {
        const all = Object.assign({}, quickSettings)
        all[graph] = withTexts(Object.assign({}, all[graph], overrides), overrides)
        quickSettings = all
    }

    function countIssue(graph, field, text, limit) {
        const trimmed = String(text).trim()
        if (!/^[+-]?\d+$/.test(trimmed))
            return {graph: graph, group: "dataRange", target: "", field: field, code: "unparsedInput"}
        const value = Number(trimmed)
        if (value <= 0)
            return {graph: graph, group: "dataRange", target: "", field: field, code: "nonPositiveCount"}
        if (value > limit)
            return {graph: graph, group: "dataRange", target: "", field: field, code: "countTooLarge"}
        return null
    }

    function applyQuickPatch(graph, patch) {
        patches = patches.concat([{graph: graph, patch: patch}])
        if (rejectWith.length > 0)
            return {accepted: false, issues: rejectWith}
        const changes = {}
        if (patch.rangeKind !== undefined) {
            changes.rangeKind = patch.rangeKind
            changes.rangeCount = 0
            if (patch.rangeKind !== "allHistory") {
                const field = patch.rangeKind === "latestRuns" ? "latestRuns" : "recentDays"
                const issue = countIssue(graph, field, patch.count, patch.rangeKind === "latestRuns" ? 1000000 : 36500)
                if (issue) return {accepted: false, issues: [issue]}
                changes.rangeCount = Number(patch.count)
            }
        }
        if (patch.window !== undefined) {
            const issue = countIssue(graph, "averagingWindow", patch.window, 3650)
            if (issue) return {accepted: false, issues: [issue]}
            changes.window = Number(patch.window)
        }
        if (patch.connection !== undefined) changes.connection = patch.connection
        if (patch.markers !== undefined) changes.markers = patch.markers
        setEntry(graph, changes)
        return {accepted: true, issues: []}
    }

    // ---- Graph drafts --------------------------------------------------------------------------------

    property var draftIssues: []
    property var calls: []
    property var graphLimits: ({
        maxRecentDays: 36500, maxLatestRuns: 1000000, maxAveragingWindow: 3650,
        minWidth: 0.5, maxWidth: 12, maxBound: 1e12,
        earliestDate: "1970-01-01", latestDate: "9999-12-31",
        earliestDateText: "01.01.1970", latestDateText: "31.12.9999", dateFormat: "dd.MM.yyyy"
    })
    property var graphDrafts: ({
        run: draft({hasRange: false, series: [], axes: []}),
        dailyPlaytime: draft({hasWindow: true, window: 3, windowText: "3",
                              series: [seriesRow("playtime", "#4dd0e1")],
                              axes: [axisRow("playtime", "zero", ["playtime"])]}),
        scenarioHistory: draft({hasLatestRuns: true, markers: true,
                                series: [seriesRow("score", "#009600"), seriesRow("accuracy", "#00ffff"),
                                         seriesRow("shots", "#ffa500"), seriesRow("hits", "#7cb342"),
                                         seriesRow("misses", "#e53935")],
                                axes: [axisRow("score", "fit", ["score"]), axisRow("accuracy", "fit", ["accuracy"]),
                                       axisRow("counts", "zero", ["shots", "hits", "misses"])]}),
        benchmarkRank: draft({series: [seriesRow("rank", undefined)], axes: []}),
        benchmarkPlaytime: draft({hasWindow: true, window: 3, windowText: "3",
                                  series: [seriesRow("playtime", "#4dd0e1")],
                                  axes: [axisRow("playtime", "zero", ["playtime"])]})
    })

    function draft(overrides) {
        return Object.assign({
            hasRange: true, hasLatestRuns: false, hasWindow: false,
            rangeKind: "allHistory", rangeCount: 0, rangeCountText: "",
            rangeStart: "", rangeEnd: "", rangeStartText: "", rangeEndText: "",
            rangeStartFormatted: "", rangeEndFormatted: "",
            anchor: false, window: 0, windowText: "", connection: "smooth", markers: false,
            series: [], axes: []
        }, overrides)
    }

    function seriesRow(key, color) {
        return {key: key, color: color, colorOverridden: false, width: 3, widthText: "3",
                widthOverridden: false, defaultWidth: 3}
    }

    function axisRow(key, mode, seriesKeys) {
        return {key: key, mode: mode, minText: "", maxText: "", seriesKeys: seriesKeys}
    }

    function record(name, args) {
        calls = calls.concat([{name: name, args: args}])
        pendingChanges = true
    }

    function callsNamed(name) { return calls.filter(c => c.name === name) }

    function change(graph, changes) {
        const all = Object.assign({}, graphDrafts)
        all[graph] = Object.assign({}, all[graph], changes)
        graphDrafts = all
    }

    function mapRows(graph, listName, key, changes) {
        return graphDrafts[graph][listName].map(r => r.key === key ? Object.assign({}, r, changes) : r)
    }

    function setGraphField(graph, group, target, field, text) {
        record("setGraphField", [graph, group, target, field, text])
        if (group === "dataRange") {
            const property = {recentDays: "rangeCountText", latestRuns: "rangeCountText", startDate: "rangeStartText",
                              endDate: "rangeEndText", averagingWindow: "windowText"}[field]
            change(graph, {[property]: text})
        } else if (group === "seriesStyle") {
            change(graph, {series: mapRows(graph, "series", target, {widthText: text, widthOverridden: true})})
        } else if (group === "axes") {
            change(graph, {axes: mapRows(graph, "axes", target, {[field === "fixedMin" ? "minText" : "maxText"]: text})})
        }
        return true
    }

    function setGraphRangeKind(graph, kind) {
        record("setGraphRangeKind", [graph, kind])
        const counts = {recentDays: 30, latestRuns: 50}
        change(graph, {rangeKind: kind, rangeCount: counts[kind] || 0, rangeCountText: String(counts[kind] || ""),
                       rangeStart: kind === "customDates" ? "2026-09-17" : "", rangeEnd: kind === "customDates" ? "2026-10-16" : "",
                       rangeStartText: kind === "customDates" ? "17.09.2026" : "",
                       rangeEndText: kind === "customDates" ? "16.10.2026" : "",
                       rangeStartFormatted: kind === "customDates" ? "17.09.2026" : "",
                       rangeEndFormatted: kind === "customDates" ? "16.10.2026" : ""})
        return true
    }

    function setGraphAnchor(graph, anchor) { record("setGraphAnchor", [graph, anchor]); change(graph, {anchor: anchor}); return true }
    function setGraphConnection(graph, connection) { record("setGraphConnection", [graph, connection]); change(graph, {connection: connection}); return true }
    function setGraphMarkers(graph, markers) { record("setGraphMarkers", [graph, markers]); change(graph, {markers: markers}); return true }

    function setSeriesColor(graph, series, color) {
        record("setSeriesColor", [graph, series, String(color)])
        change(graph, {series: mapRows(graph, "series", series, {color: color, colorOverridden: true})})
        return true
    }

    function clearSeriesOverride(graph, series, field) {
        record("clearSeriesOverride", [graph, series, field])
        return true
    }

    function setAxisMode(graph, axis, mode) {
        record("setAxisMode", [graph, axis, mode])
        change(graph, {axes: mapRows(graph, "axes", axis, {mode: mode, minText: mode === "fixed" ? "0" : "", maxText: mode === "fixed" ? "0" : ""})})
        return true
    }

    function resetGraph(graph) { record("resetGraph", [graph]); return true }
}
