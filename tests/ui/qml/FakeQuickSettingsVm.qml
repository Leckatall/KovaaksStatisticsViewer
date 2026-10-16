import QtQuick

// The slice of SettingsViewModel that the graph gear popups and the Graphs navigation use, with
// notifying properties so bindings in the popup follow committed changes the way the real one does.
// Count text is validated like the real view model: a whole number from 1 to the field's limit.
QtObject {
    id: vm

    property string selectedGraph: "run"

    // What the Settings window needs besides the graph surface.
    property url kovaaksDir: "file:///C:/Kovaaks"
    property url profilePath: "file:///C:/Profile/profile.pb"
    property bool profileLoaded: false
    property var allSeriesConfigs: []
    property var allAxes: []
    property var issues: []
    property var rawInputs: []
    property string runAxisLabelSeriesId: ""
    property bool pendingChanges: false
    property int beginVisitCalls: 0
    property int endVisitCalls: 0
    function beginVisit() { beginVisitCalls++ }
    function endVisit() { endVisitCalls++; pendingChanges = false }
    function save() { pendingChanges = false; return {accepted: true, issues: []} }
    function revertChanges() { pendingChanges = false }
    function setRunAxisLabelSeriesId(id) {}
    function setKovaaksDir(dir) {}
    function setProfilePath(path) {}
    property var patches: []
    // Non-empty: the next applyQuickPatch is rejected with these issues and changes nothing.
    property var rejectWith: []

    property var quickSettings: ({
        run: entry({hasRange: false, hasLatestRuns: false, hasWindow: false, markers: false}),
        dailyPlaytime: entry({hasWindow: true, window: 3}),
        scenarioHistory: entry({hasLatestRuns: true, markers: true}),
        benchmarkRank: entry({}),
        benchmarkPlaytime: entry({hasWindow: true, window: 3})
    })

    function entry(overrides) {
        return Object.assign({
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
        }, overrides)
    }

    function setEntry(graph, overrides) {
        const all = Object.assign({}, quickSettings)
        all[graph] = Object.assign({}, all[graph], overrides)
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
}
