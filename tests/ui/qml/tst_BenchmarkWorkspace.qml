import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

// W2 + W3. Both suites live here because W3 modifies this file (per the plan).
//
// BenchmarkWorkspace.qml is loaded at runtime with Qt.createComponent rather than
// referenced as a literal type, so a missing production file fails these tests
// with a clear assertion instead of a compile error that would also break
// discovery for every other tst_*.qml in this directory. Until W2/W3 land the
// component does not resolve and every test below goes red at makeWorkspace().

Item {
    id: root
    width: 1200
    height: 800

    property int managerRequests: 0
    property var editRequests: []

    Component {
        id: tierBarComponent
        TierBar { objectName: "probe"; height: 16 }
    }

    Component {
        id: progressGridComponent
        BenchmarkProgressGrid { }
    }

    Component {
        id: largeFontHostComponent
        Control {
            anchors.fill: parent
            font.pixelSize: Math.round(Qt.application.font.pixelSize * 1.5)
        }
    }

    // =====================================================================
    // W2 - truthful selector, empty / unavailable / incomplete / trackable
    // =====================================================================
    TestCase {
        id: stateCase
        name: "BenchmarkWorkspaceStateTest"
        when: windowShown
        width: 1200
        height: 800
        visible: true

        function instantiateWorkspace(parentItem) {
            // RED: the BenchmarkWorkspace type does not resolve until W2/W3 land,
            // so this throws / returns null and the calling test fails cleanly.
            let obj = null
            try {
                obj = Qt.createQmlObject(
                    'import QtQuick; import "../../../src/ui/qml"; BenchmarkWorkspace { }',
                    parentItem, "dyn_BenchmarkWorkspace")
            } catch (e) {
                verify(false, "BenchmarkWorkspace.qml did not load: " + e)
            }
            verify(obj !== null, "BenchmarkWorkspace failed to instantiate")
            return obj
        }

        function makeWorkspace(fixtureOverrides, w, h) {
            const ws = instantiateWorkspace(root)
            ws.trackingVm = TestDoubles.makeFakeTrackingVm(fixtureOverrides || {})
            ws.width = w || 1200
            ws.height = h || 800
            root.managerRequests = 0
            root.editRequests = []
            ws.manageBenchmarksRequested.connect(function () { root.managerRequests++ })
            ws.editBenchmarkRequested.connect(function (filename, benchmarkId) {
                root.editRequests = root.editRequests.concat([[filename, benchmarkId]])
            })
            return ws
        }
        function find(ws, name) { return ItemLookup.findByObjectName(ws, name) }
        function findAllPrefixed(ws, prefix) { return ItemLookup.findByObjectNamePrefix(ws, prefix, []) }

        // -- always-visible header ---------------------------------------

        function test_headerSelectorIsShownWheneverAnyEntryExists_data() {
            return [
                {tag: "problems", fx: TestDoubles.trackingFixtureProblemsOnly()},
                {tag: "noselection", fx: TestDoubles.trackingFixtureNoSelection()},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable()},
                {tag: "incomplete", fx: TestDoubles.trackingFixtureIncomplete()},
                {tag: "trackable", fx: TestDoubles.trackingFixtureTrackable()}
            ]
        }

        function test_headerSelectorIsShownWheneverAnyEntryExists(data) {
            const ws = makeWorkspace(data.fx)
            const selector = find(ws, "benchmarkSelector")
            verify(selector !== null && selector.visible, "header benchmark selector missing")
            verify(String(selector.Accessible.name).length > 0, "selector needs an accessible name")
        }

        // with nothing installed there is nothing to select; the body alone
        // explains and acts.
        function test_emptyLibraryHidesTheHeader() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureEmptyLibrary())
            const header = find(ws, "benchmarkHeader")
            verify(header !== null, "header row missing")
            compare(header.visible, false, "an empty library must not show an empty selector")
        }

        // never two controls that open the Benchmark Manager at once.
        function test_exactlyOneManagerActionPerState_data() {
            return [
                {tag: "empty", fx: TestDoubles.trackingFixtureEmptyLibrary(), action: "createOrImportButton"},
                {tag: "problems", fx: TestDoubles.trackingFixtureProblemsOnly(), action: "manageBenchmarkFilesButton"},
                {tag: "noselection", fx: TestDoubles.trackingFixtureNoSelection(), action: "manageBenchmarksButton"},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable(), action: "repairButton"},
                {tag: "incomplete", fx: TestDoubles.trackingFixtureIncomplete(), action: "editBenchmarkButton"},
                {tag: "trackable", fx: TestDoubles.trackingFixtureTrackable(), action: "manageBenchmarksButton"}
            ]
        }

        function test_exactlyOneManagerActionPerState(data) {
            const ws = makeWorkspace(data.fx)
            const actions = ["manageBenchmarksButton", "createOrImportButton", "manageBenchmarkFilesButton",
                             "repairButton", "editBenchmarkButton"]
            const shown = actions.filter(function (name) {
                const item = find(ws, name)
                return item !== null && item.visible
            })
            compare(shown.length, 1, "visible manager actions: " + shown.join(", "))
            compare(shown[0], data.action)
        }

        // Visible, non-empty text of every item, ignoring a control's own content
        // item (a Button's label repeats the Button's text).
        function visibleTexts(item, out) {
            out = out || []
            if (!item || !item.visible)
                return out
            const t = item.text === undefined || item.text === null ? "" : String(item.text)
            const parentText = item.parent && item.parent.text !== undefined ? String(item.parent.text) : null
            if (t.length > 0 && t !== parentText && item.width > 0)
                out.push(t)
            for (const child of item.children || [])
                visibleTexts(child, out)
            return out
        }

        // no status or instruction message appears twice on the screen.
        function test_noMessageIsShownTwice_data() {
            return [
                {tag: "empty", fx: TestDoubles.trackingFixtureEmptyLibrary()},
                {tag: "problems", fx: TestDoubles.trackingFixtureProblemsOnly()},
                {tag: "noselection", fx: TestDoubles.trackingFixtureNoSelection()},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable()}
            ]
        }

        function test_noMessageIsShownTwice(data) {
            const ws = makeWorkspace(data.fx)
            wait(0)
            const texts = visibleTexts(ws)
            const seen = {}
            for (const t of texts) {
                verify(!seen[t], "shown twice: '" + t + "' in " + JSON.stringify(texts))
                seen[t] = true
            }
            // Near-duplicates: at most one visible line may ask the user to select a
            // benchmark, and at most one may say there are none.
            const selectLines = texts.filter(function (t) { return /select a benchmark/i.test(t) })
            verify(selectLines.length <= 1, "repeated selection prompts: " + JSON.stringify(selectLines))
            const noneLines = texts.filter(function (t) { return /no benchmark/i.test(t) })
            verify(noneLines.length <= 1, "repeated 'no benchmarks' messages: " + JSON.stringify(noneLines))
        }

        function test_headerStatusLineOnlyWhileTracking_data() {
            return [
                {tag: "problems", fx: TestDoubles.trackingFixtureProblemsOnly(), shown: false},
                {tag: "noselection", fx: TestDoubles.trackingFixtureNoSelection(), shown: false},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable(), shown: false},
                {tag: "incomplete", fx: TestDoubles.trackingFixtureIncomplete(), shown: false},
                {tag: "trackable", fx: TestDoubles.trackingFixtureTrackable(), shown: true}
            ]
        }

        function test_headerStatusLineOnlyWhileTracking(data) {
            const ws = makeWorkspace(data.fx)
            const status = find(ws, "benchmarkClassificationLabel")
            verify(status !== null, "header status label missing")
            compare(status.visible, data.shown,
                    "every other state explains itself in its body, so the header must not repeat it")
        }

        function test_manageBenchmarksButtonEmitsRequest() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureNoSelection())
            mouseClick(find(ws, "manageBenchmarksButton"))
            compare(root.managerRequests, 1, "Manage Benchmarks must raise manageBenchmarksRequested")
        }

        // -- selector shows problem rows but disables them ---------------

        function test_problemEntriesVisibleButNotActivatable() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureNoSelection())
            const rows = findAllPrefixed(ws, "selectorEntry_")
            verify(rows.length >= 2, "selector must list loaded and problem entries")

            const problem = find(ws, "selectorEntry_broken.json")
            verify(problem !== null, "the Invalid entry must remain visible in the selector")
            compare(problem.enabled, false, "Invalid/Unsupported entries must be disabled")

            mouseClick(problem)
            compare(ws.trackingVm.selectBenchmarkCalls.length, 0,
                    "clicking a disabled problem row must not select it")
        }

        function test_selectorLabelsOnlyExceptions() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureNoSelection())
            const entry = find(ws, "selectorEntry_Voltaic S3")
            verify(entry !== null, "trackable entry missing")
            compare(entry.text, "Voltaic S3", "a trackable entry shows only its name")
            const trackableChip = find(ws, "selectorEntryLabel_Voltaic S3")
            verify(trackableChip !== null)
            compare(trackableChip.visible, false, "trackable is the normal state and carries no label")
            const problemChip = find(ws, "selectorEntryLabel_broken.json")
            verify(problemChip !== null)
            compare(problemChip.visible, true, "an exception keeps its label")
            compare(problemChip.kind.toLowerCase(), "invalid")
        }

        // Every exception kind the real view model emits (lowercase) keeps a
        // visible, labelled pill; only trackable rows show the bare name.
        function test_selectorLabelsEveryExceptionKind_data() {
            return [
                {tag: "incomplete", name: "Half Built", selectable: true},
                {tag: "invalid", name: "broken.json", selectable: false},
                {tag: "unsupported", name: "Future Format", selectable: false}
            ]
        }

        function test_selectorLabelsEveryExceptionKind(data) {
            const ws = makeWorkspace({
                state: "NoSelection",
                selectorEntries: [
                    {id: "bench-A", name: "Voltaic S3", classification: "trackable", selectable: true},
                    {id: data.selectable ? "bench-X" : "", name: data.name,
                     classification: data.tag, selectable: data.selectable}
                ]
            })
            const chip = find(ws, "selectorEntryLabel_" + data.name)
            verify(chip !== null, data.tag + " entry has no label")
            compare(chip.visible, true, "the " + data.tag + " benchmark must keep its label")
            compare(chip.kind, data.tag)
            verify(chip.width > 0 && chip.height > 0, "the label must have a size")
            const row = find(ws, "selectorEntry_" + data.name)
            compare(row.enabled, data.selectable)
            verify(row.rightPadding >= chip.width, "the name must leave room for the label")
            compare(find(ws, "selectorEntryLabel_Voltaic S3").visible, false)
        }

        // A row with a warning chip carries a control that opens the Benchmark Manager
        // on that benchmark; a trackable row stays name-only.
        function test_onlyWarningRowsOfferTheManager_data() {
            return [
                {tag: "trackable", name: "Voltaic S3", shown: false},
                {tag: "incomplete", name: "Half Built", shown: true, verb: "Edit"},
                {tag: "invalid", name: "broken.json", shown: true, verb: "Show"},
                {tag: "unsupported", name: "Future Format", shown: true, verb: "Show"}
            ]
        }

        function test_onlyWarningRowsOfferTheManager(data) {
            const selectable = data.tag === "trackable" || data.tag === "incomplete"
            const ws = makeWorkspace({
                state: "NoSelection",
                selectorEntries: [
                    {id: selectable ? "bench-X" : "", name: data.name, filename: "x.json",
                     classification: data.tag, selectable: selectable}
                ]
            })
            const control = find(ws, "selectorEntryEdit_" + data.name)
            verify(control !== null, "every row has the control, shown or not")
            compare(control.visible, data.shown)
            const chip = find(ws, "selectorEntryLabel_" + data.name)
            compare(chip.visible, data.shown, "the control goes with the warning chip")
            if (!data.shown) return
            verify(control.width > 0 && control.height > 0, "the control must have a size")
            const name = String(control.Accessible.name)
            verify(name.indexOf(data.verb) === 0 && name.indexOf(data.name) !== -1
                   && name.indexOf("Benchmark Manager") !== -1,
                   "the accessible name says what the control does: " + name)
            compare(String(control.ToolTip.text), name, "the tooltip says the same")
            // The control sits beside the chip, inside the row, and the name leaves room for both.
            const row = find(ws, "selectorEntry_" + data.name)
            const controlX = control.mapToItem(row, 0, 0).x
            const chipRight = chip.mapToItem(row, chip.width, 0).x
            verify(controlX >= chipRight, "the control sits right of the chip")
            verify(controlX + control.width <= row.width, "the control stays inside the row")
            verify(row.rightPadding >= chip.width + control.width, "the name leaves room for chip and control")
        }

        function test_warningControlOpensTheManagerWithoutSelecting_data() {
            return [
                {tag: "incomplete", entry: {id: "bench-I", name: "Half Built", filename: "half.json",
                                            classification: "incomplete", selectable: true}},
                {tag: "invalid-without-id", entry: {id: "", name: "broken.json", filename: "broken.json",
                                                    classification: "invalid", selectable: false}},
                {tag: "unsupported", entry: {id: "bench-U", name: "Future Format", filename: "future.json",
                                             classification: "unsupported", selectable: false}}
            ]
        }

        function test_warningControlOpensTheManagerWithoutSelecting(data) {
            const ws = makeWorkspace({
                state: "NoSelection",
                selectorEntries: [
                    {id: "bench-A", name: "Voltaic S3", filename: "voltaic-s3.json",
                     classification: "trackable", selectable: true},
                    data.entry
                ]
            })
            const control = find(ws, "selectorEntryEdit_" + data.entry.name)
            verify(control !== null && control.visible)
            mouseClick(control)
            compare(root.editRequests.length, 1, "the control must raise editBenchmarkRequested")
            compare(root.editRequests[0], [data.entry.filename, data.entry.id])
            compare(ws.trackingVm.selectBenchmarkCalls.length, 0, "the control must not select the row")
            compare(root.managerRequests, 0, "the control opens the manager on its benchmark, not plainly")
        }

        // showing the workspace with nothing selected asks the view model for
        // the default (remembered or only) benchmark; which one is the use case's call.
        function test_shownWithoutSelectionRequestsDefaultBenchmark() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureNoSelection())
            tryVerify(function () { return ws.trackingVm.selectDefaultBenchmarkCalls >= 1 }, 1000,
                      "a visible workspace with no selection must request the default benchmark")
            compare(ws.trackingVm.selectBenchmarkCalls.length, 0,
                    "the workspace must not pick a benchmark itself")
        }

        function test_hiddenWorkspaceRequestsDefaultOnlyWhenShown() {
            const ws = instantiateWorkspace(root)
            ws.visible = false
            ws.trackingVm = TestDoubles.makeFakeTrackingVm(TestDoubles.trackingFixtureNoSelection())
            wait(50)
            compare(ws.trackingVm.selectDefaultBenchmarkCalls, 0,
                    "a hidden (not yet opened) workspace must not select anything")
            ws.visible = true
            compare(ws.trackingVm.selectDefaultBenchmarkCalls, 1,
                    "opening the workspace must request the default benchmark at once")
        }

        function test_existingSelectionIsLeftAlone_data() {
            return [
                {tag: "trackable", fx: TestDoubles.trackingFixtureTrackable()},
                {tag: "empty", fx: TestDoubles.trackingFixtureEmptyLibrary()},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable()}
            ]
        }

        function test_existingSelectionIsLeftAlone(data) {
            const ws = makeWorkspace(data.fx)
            wait(50)
            compare(ws.trackingVm.selectDefaultBenchmarkCalls, 0,
                    "only the no-selection state asks for a default benchmark")
        }

        // -- state bodies ----------------------------------------------------

        function test_emptyLibraryOffersCreateOrImport() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureEmptyLibrary())
            verify(find(ws, "emptyLibraryBody") !== null, "empty-library body missing")
            const action = find(ws, "createOrImportButton")
            verify(action !== null && action.visible,
                   "empty library must offer 'Create or import a benchmark'")
            mouseClick(action)
            compare(root.managerRequests, 1, "the empty-state action opens the Benchmark Manager")
            verify(find(ws, "noSelectionBody") === null,
                   "empty library must not prompt for an impossible selection")
        }

        function test_problemsOnlyOffersManageFiles() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureProblemsOnly())
            verify(find(ws, "problemsOnlyBody") !== null, "problems-only body missing")
            verify(find(ws, "manageBenchmarkFilesButton") !== null,
                   "problems-only body must offer 'Manage benchmark files'")
        }

        function test_noSelectionPromptsToSelect() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureNoSelection())
            verify(find(ws, "noSelectionBody") !== null, "no-selection body missing")
        }

        function test_unavailableShowsLastKnownNameAndRepairPathAndNoStaleData() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureUnavailable())
            verify(find(ws, "unavailableBody") !== null, "unavailable body missing")

            const name = find(ws, "unavailableName")
            verify(name !== null, "unavailable body must show the last-known name")
            verify(String(name.text).indexOf("Voltaic S3") !== -1, "last-known display name must be shown")
            verify(find(ws, "repairButton") !== null, "unavailable body must offer the manager repair path")

            compare(find(ws, "benchmarkStatusSummary"), null, "no stale status summary in Unavailable")
            compare(find(ws, "rankHistoryCard"), null, "no stale rank graph in Unavailable")
            compare(find(ws, "playtimeHistoryCard"), null, "no stale playtime graph in Unavailable")
            compare(find(ws, "benchmarkProgressGrid"), null, "no stale progress grid in Unavailable")
        }

        function test_incompleteShowsEveryIssueAndOnlySafeFacts() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureIncomplete())
            verify(find(ws, "incompleteBody") !== null, "incomplete body missing")

            const issues = findAllPrefixed(ws, "completenessIssue_")
            compare(issues.length, 2, "every completeness issue must be listed")
            verify(find(ws, "editBenchmarkButton") !== null, "incomplete body must offer 'Edit benchmark'")

            verify(find(ws, "incompleteTotalPlaytime") !== null,
                   "incomplete body may show total playtime as a safe fact")

            const attained = find(ws, "incompleteAttainedRank")
            if (attained !== null) {
                verify(String(attained.text) !== "0" && String(attained.text) !== "Unranked",
                       "incomplete attained rank must read as unavailable, not a plausible value")
            }
            compare(find(ws, "rankHistoryCard"), null,
                    "incomplete definitions must not render the average-rank chart")
            verify(find(ws, "progressRow_sc-1_bar") !== null,
                   "the incomplete body lists its scenarios in the progress grid too")
        }

        function test_trackableShowsAllSeparatelyLabelledSummaryValues() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            verify(find(ws, "benchmarkStatusSummary") !== null, "trackable state must show the status summary")

            const attained = find(ws, "summaryAttainedRank")
            const completed = find(ws, "summaryCompletedRank")
            const avg = find(ws, "summaryAverageRank")
            const playtime = find(ws, "summaryTotalPlaytime")
            const nextTier = find(ws, "summaryNextTier")
            verify(attained !== null && completed !== null && avg !== null
                   && playtime !== null && nextTier !== null,
                   "attained/completed/average rank, total playtime and next tier must each be "
                   + "a separately labelled value")

            compare(attained.text, "Gold")
            compare(completed.text, "Silver")
            compare(avg.text, "2.75")
            compare(playtime.text, "5 h 40 min")
            compare(nextTier.text, "Platinum")

            verify(String(attained.Accessible.description).toLowerCase().indexOf("attained") !== -1,
                   "attained-rank value needs a description naming its metric")
            verify(attained !== completed, "attained and completed rank must be distinct elements")
        }

        // the average rank is drawn on the tier bar, with the completed rank as a tick
        // and the number in the tooltip.
        function test_summaryAverageRankIsATierBarWithACompletedRankTick() {
            const fx = TestDoubles.trackingFixtureTrackable()
            fx.averageRank = "2.75"
            fx.averageRankFills = [1, 1, 0.75, 0]
            fx.completedRankPosition = 2      // Silver, the second tier: the end of its chunk
            const ws = makeWorkspace(fx)
            wait(0)
            const area = find(ws, "summaryAverageRank")
            const bar = find(ws, "summaryAverageRankBar")
            verify(bar !== null && bar.visible, "the average rank is a tier bar")
            compare(bar.chunkCount, 4, "one chunk per tier")
            for (let i = 0; i < 4; ++i) {
                const chunk = find(bar, bar.objectName + "_chunk_" + i)
                const fill = find(bar, bar.objectName + "_fill_" + i)
                fuzzyCompare(fill.width, chunk.width * fx.averageRankFills[i], 1, "chunk " + i)
            }
            const tick = find(bar, bar.objectName + "_tick")
            verify(tick.visible, "the completed rank is ticked")
            const tickX = tick.mapToItem(ws, tick.width / 2, 0).x
            const silverEnd = find(bar, bar.objectName + "_chunk_1")
            const goldStart = find(bar, bar.objectName + "_chunk_2")
            verify(tickX >= silverEnd.mapToItem(ws, silverEnd.width, 0).x - 1
                   && tickX <= goldStart.mapToItem(ws, 0, 0).x + 1, "the tick sits at the end of Silver's chunk")
            compare(tick.ToolTip.text, "Completed rank: Silver")
            compare(area.ToolTip.text, "Average rank 2.75", "the number stays in the tooltip")
            verify(find(ws, "summaryAttainedRank").visible && find(ws, "summaryNextTier").visible,
                   "attained rank and next tier stay visible as text")
            // The bar does not push the right-hand column off its half of the card.
            const summary = find(ws, "benchmarkStatusSummary")
            const completed = find(ws, "summaryCompletedRank")
            tryVerify(() => completed.mapToItem(summary, 0, 0).x < summary.width / 2 + 140, 2000,
                      "the right-hand fields stay in the card's right half, not pushed across it: "
                      + completed.mapToItem(summary, 0, 0).x + " of " + summary.width)
        }

        function test_summaryWithoutACompletedRankHasNoTick() {
            const fx = TestDoubles.trackingFixtureTrackable()
            fx.averageRankFills = [0.87, 0, 0, 0]
            fx.completedRankPosition = -1
            const ws = makeWorkspace(fx)
            const bar = find(ws, "summaryAverageRankBar")
            const tick = find(bar, bar.objectName + "_tick")
            verify(tick === null || !tick.visible, "nothing completed, no tick")
        }

        // the grid replaces the blocker list: a blocking row carries a marker in
        // the next tier's colour that says what it blocks.
        function test_blockingRowsAreMarkedInTheNextTierColour() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const markers = findAllPrefixed(ws, "progressRow_").filter(function (item) {
                return /_blocker$/.test(item.objectName)
            })
            compare(markers.length, 2, "one marker per blocking row")

            const marker = find(ws, "progressRow_sc-1w4ts_blocker")
            verify(marker !== null && marker.visible, "the blocking row is marked")
            verify(String(marker.Accessible.name).indexOf("Blocks Platinum") !== -1,
                   "the marker says which tier it blocks: " + marker.Accessible.name)
            verify(String(marker.ToolTip.text).indexOf("Blocks Platinum") !== -1, "and its tooltip does too")
            verify(Qt.colorEqual(marker.children[0].color, "#50dcdc"), "the marker uses the next tier's colour")
            verify(find(ws, "progressRow_sc-pasu_blocker") === null, "a row that blocks nothing has no marker")
        }

        function test_headingCountsBlockersAndMatchesTheMarkedRows() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const markers = findAllPrefixed(ws, "progressRow_").filter(function (item) {
                return /_blocker$/.test(item.objectName)
            })
            const t = String(find(ws, "nextTierProgress").text)
            compare(t.replace(/<[^>]*>/g, ""), markers.length + " scenarios block Platinum")
            verify(t.indexOf("#50dcdc") !== -1, "the tier name keeps its colour")
        }

        function test_headingSingularAndNoNextTierAndUnranked() {
            const fx = TestDoubles.trackingFixtureTrackable()
            fx.nextTierBlockerCount = 1
            let ws = makeWorkspace(fx)
            compare(String(find(ws, "nextTierProgress").text).replace(/<[^>]*>/g, ""), "1 scenario blocks Platinum")

            const top = TestDoubles.trackingFixtureTrackable()
            top.hasNextTier = false
            top.nextTier = "Highest tier attained"
            top.nextTierColor = ""
            top.nextTierBlockerCount = 0
            ws = makeWorkspace(top)
            compare(String(find(ws, "nextTierProgress").text), "Highest tier attained")

            const none = TestDoubles.trackingFixtureTrackable()
            none.attainedRank = "Unranked"
            none.attainedRankColor = ""
            none.nextTier = "Bronze"
            none.nextTierColor = "#cd7f32"
            none.nextTierBlockerCount = 4
            ws = makeWorkspace(none)
            compare(String(find(ws, "nextTierProgress").text).replace(/<[^>]*>/g, ""),
                    "Not ranked yet: 4 scenarios block Bronze")
        }

        // A non-trackable benchmark has no rank to progress from, so the heading must not
        // show. The fixtures carry stale next-tier values on purpose: a heading that read
        // them regardless of the state would render text.
        function test_headingIsHiddenWhenNotTrackable_data() {
            return [
                {tag: "incomplete", fx: TestDoubles.trackingFixtureIncomplete(), body: "incompleteBody"},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable(), body: "unavailableBody"}
            ]
        }
        function test_headingIsHiddenWhenNotTrackable(data) {
            const fx = Object.assign({}, data.fx, {
                hasNextTier: true, nextTier: "Gold", nextTierColor: "#ffc828", nextTierBlockerCount: 3
            })
            verify(String(fx.state).toLowerCase() !== "trackable", "the fixture must not be trackable")
            verify(fx.summaryAvailable === false, "a non-trackable fixture has no summary")

            const ws = makeWorkspace(fx)
            verify(find(ws, data.body) !== null, "the " + data.tag + " body must be shown")
            verify(find(ws, "benchmarkStatusSummary") === null, "no status summary when not trackable")
            const heading = find(ws, "nextTierProgress")
            verify(heading === null || !heading.visible,
                   "the next-tier heading must not show: " + (heading ? heading.text : ""))
        }
    }

    // =====================================================================
    // W3 - aligned histories + accessible responsive breakdown
    // =====================================================================
    TestCase {
        id: contentCase
        name: "BenchmarkWorkspaceContentTest"
        when: windowShown
        width: 1200
        height: 800
        visible: true

        function instantiateWorkspace(parentItem) {
            let obj = null
            try {
                obj = Qt.createQmlObject(
                    'import QtQuick; import "../../../src/ui/qml"; BenchmarkWorkspace { }',
                    parentItem, "dyn_BenchmarkWorkspace")
            } catch (e) {
                verify(false, "BenchmarkWorkspace.qml did not load: " + e)
            }
            verify(obj !== null, "BenchmarkWorkspace failed to instantiate")
            return obj
        }

        function makeWorkspace(fixtureOverrides, w, h) {
            const ws = instantiateWorkspace(root)
            ws.trackingVm = TestDoubles.makeFakeTrackingVm(fixtureOverrides || {})
            ws.width = w || 1200
            ws.height = h || 800
            return ws
        }
        function find(ws, name) { return ItemLookup.findByObjectName(ws, name) }
        function findAllPrefixed(ws, prefix) { return ItemLookup.findByObjectNamePrefix(ws, prefix, []) }

        function scrollColumn(ws) {
            const col = find(ws, "trackableScrollColumn")
            verify(col !== null, "no trackableScrollColumn - the scrollable content order is not built")
            return col
        }

        function indexInColumn(col, objectName) {
            const kids = col.children
            for (let i = 0; i < kids.length; ++i) {
                if (ItemLookup.findByObjectName(kids[i], objectName) !== null)
                    return i
            }
            return -1
        }

        // -- scroll order --------------------------------------------------

        function test_trackableContentIsInSpecifiedScrollOrder() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const col = scrollColumn(ws)
            const summary = indexInColumn(col, "benchmarkStatusSummary")
            const progress = indexInColumn(col, "nextTierProgress")
            const rank = indexInColumn(col, "rankHistoryCard")
            const playtime = indexInColumn(col, "playtimeHistoryCard")
            const grid = indexInColumn(col, "benchmarkProgressGrid")
            verify(summary >= 0 && progress >= 0 && rank >= 0 && playtime >= 0 && grid >= 0,
                   "all content blocks must be present in the scroll column")
            verify(summary < progress, "status summary comes before next-tier progress")
            verify(grid === progress, "the progress grid sits with the next-tier count")
            verify(grid < rank, "the progress grid comes before the history cards")
            verify(rank < playtime, "average-rank card comes before rolling-playtime card")
            wait(0)
            const label = find(ws, "nextTierProgress")
            const gridItem = find(ws, "benchmarkProgressGrid")
            verify(label.mapToItem(ws, 0, label.height).y <= gridItem.mapToItem(ws, 0, 0).y + 0.5,
                   "the 'N of M scenarios at the next tier' line stays above the grid")
        }

        function test_horizontalScrollIsNotTheStrategy() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 800, 600)
            const flick = find(ws, "benchmarkWorkspaceScroll")
            verify(flick !== null, "workspace must have a vertical scroll surface")
            if (flick.contentWidth !== undefined && flick.width !== undefined)
                verify(flick.contentWidth <= flick.width + 1,
                       "content must not require horizontal scrolling at 800x600")
        }

        // -- history cards: empty vs non-empty ---------------------------

        function test_emptyHistoryCardShowsExplicitTextAndNoLine() {
            const fx = TestDoubles.trackingFixtureTrackable()
            fx.rankHistory = TestDoubles.makeFakeHistoryModel({
                metricName: "Personal-best average rank", hasData: false,
                emptyStateText: "No ranked history yet"
            })
            const ws = makeWorkspace(fx)
            verify(find(ws, "rankHistoryCard") !== null, "rank history card missing")
            const empty = find(ws, "rankHistoryEmptyLabel")
            verify(empty !== null && empty.visible, "empty history card must show explicit empty text")
            compare(empty.text, "No ranked history yet")
            const canvas = find(ws, "rankHistoryCanvas")
            if (canvas !== null)
                compare(canvas.visible, false, "no misleading line when the series is absent")
        }

        function test_nonEmptyHistoryCardsAreSeparateWithNamedAccessibleLines() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const rankCard = find(ws, "rankHistoryCard")
            const playtimeCard = find(ws, "playtimeHistoryCard")
            verify(rankCard !== null && playtimeCard !== null, "both history cards must exist")
            verify(rankCard !== playtimeCard, "rank and playtime must be separate cards")

            const rankLine = find(ws, "rankHistoryLineName")
            const playtimeLine = find(ws, "playtimeHistoryLineName")
            verify(rankLine !== null && String(rankLine.text).toLowerCase().indexOf("average rank") !== -1,
                   "the rank line must be identified as personal-best average rank")
            verify(playtimeLine !== null
                   && String(playtimeLine.text).toLowerCase().indexOf("playtime") !== -1,
                   "the playtime line must be identified as three-day average playtime")

            // Card titles share the Scenarios-tab chart title style (ChartTitle).
            for (const line of [rankLine, playtimeLine]) {
                compare(line.horizontalAlignment, Text.AlignLeft)
                compare(line.font.weight, Font.DemiBold)
                compare(line.font.pixelSize, 14)
            }

            verify(String(rankCard.Accessible.description).toLowerCase().indexOf("rank") !== -1,
                   "rank card description must include its metric name")
            verify(String(playtimeCard.Accessible.description).toLowerCase().indexOf("playtime") !== -1,
                   "playtime card description must include its metric name")
        }

        // -- progress grid: column order and tier headers -----------------

        function test_pbDateAndPlaysPrecedeTheBarAndNextTierFollowsIt() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 1200, 800)
            wait(0)
            // Unscrolled, so the pinned columns sit where the layout puts them.
            find(ws, "progressGridFlick").contentX = 0
            wait(0)
            const order = ["progressHeader_category", "progressHeader_subcategory", "progressHeader_scenario",
                           "progressHeader_pbDate", "progressHeader_plays", "progressHeader_progress",
                           "progressHeader_next"]
            let previousRight = -1
            for (const name of order) {
                const header = find(ws, name)
                verify(header !== null, name + " missing")
                const left = header.mapToItem(ws, 0, 0).x
                verify(Math.abs(left - previousRight) <= 0.5 || previousRight < 0,
                       name + " follows the previous column directly")
                previousRight = left + header.width
            }
            const bar = find(ws, "progressRow_sc-1w4ts_bar")
            const plays = find(ws, "progressHeader_plays")
            verify(bar.mapToItem(ws, 0, 0).x >= plays.mapToItem(ws, plays.width, 0).x,
                   "the bar sits right of Plays")
        }

        function test_tierHeadersShowOnlyTheirNameInAReadableTierColour() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 1200, 800)
            wait(0)
            const grid = find(ws, "benchmarkProgressGrid")
            const tiers = grid.tiers
            const seen = []
            for (let i = 0; i < tiers.length; ++i) {
                const header = find(ws, "progressTierHeader_" + i)
                const name = find(ws, "progressTierHeader_" + i + "_name")
                compare(name.text, tiers[i].name)
                verify(name.font.bold, "tier names are bold")
                // No swatch: the header holds only its underline and its name.
                compare(header.children.length, 2, "tier " + i + " header has no colour swatch")
                const underline = header.children[0]
                verify(Qt.colorEqual(underline.color, tiers[i].barColor), "tier " + i + " keeps its tier underline")
                verify(contrast(name.color, header.color) >= 4.5,
                       "tier " + i + " name reads on its header: " + name.color + " on " + header.color)
                for (const other of seen)
                    verify(!Qt.colorEqual(other, name.color), "tier " + i + " name has its own colour")
                seen.push(name.color)
            }
        }

        function test_readableTierColourKeepsColoursThatReadAndBlendsTheRest() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 1200, 800)
            const grid = find(ws, "benchmarkProgressGrid")
            // Dark header: gold reads as is; the dark iron grey is lightened until it reads.
            verify(Qt.colorEqual(grid.readableOn("#ffc828", "#3c3c3c", "#ffffff"), "#ffc828"))
            const iron = grid.readableOn("#82828c", "#3c3c3c", "#ffffff")
            verify(!Qt.colorEqual(iron, "#82828c") && contrast(iron, "#3c3c3c") >= 4.5, "iron is lightened")
            verify(iron.r < 0.9, "iron is lightened only as far as it needs: " + iron)
            // Light header: gold is darkened toward the dark text colour instead.
            const gold = grid.readableOn("#ffc828", "#efefef", "#000000")
            verify(contrast(gold, "#efefef") >= 4.5 && gold.r > gold.b, "gold stays a gold on light: " + gold)
            verify(Qt.colorEqual(grid.readableOn("", "#3c3c3c", "#ffffff"), "#ffffff"), "no colour: the fallback")
        }

        // Too little width: the chunks give up their padding around the widest tier name
        // before any scenario name shortens, and the grid scrolls only after both.
        function test_chunkPaddingGivesWayBeforeScenarioNames() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 1200, 800)
            const grid = find(ws, "benchmarkProgressGrid")
            compare(grid.tiers.length, 4)
            const m = {category: 100, subcategory: 0, scenario: 300, scenarioMinimum: 120, pbDate: 80,
                       plays: 50, next: 100, chunk: 100, chunkMinimum: 84}
            // fixed columns 630; bar comfortable 4*100+16 = 416, minimum 4*84+16 = 352
            let cols = grid._layoutColumns(1046, m)
            compare(cols.progress.width, 416, "room to spare: comfortable chunks")
            compare(cols.scenario.width, 300)
            cols = grid._layoutColumns(1000, m)
            compare(cols.progress.width, 370, "the chunk padding shrinks first")
            compare(cols.scenario.width, 300, "scenario names keep their width while padding is left")
            cols = grid._layoutColumns(950, m)
            compare(cols.progress.width, 352, "chunks stop at the widest name")
            compare(cols.scenario.width, 268, "then the scenario names shorten")
            cols = grid._layoutColumns(700, m)
            compare(cols.progress.width, 352)
            compare(cols.scenario.width, 120)
            verify(cols.total > 700, "and then the grid scrolls")
        }

        function contrast(a, b) {
            const linear = c => c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)
            const lum = c => 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b)
            const la = lum(Qt.color(a)), lb = lum(Qt.color(b))
            return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05)
        }

        // -- progress grid: rows, values, group cells ---------------------

        function test_scenarioRowExposesEveryValueInItsOwnCell() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const row = find(ws, "progressRow_sc-1w4ts")
            verify(row !== null, "row for sc-1w4ts missing")
            compare(find(ws, "progressRow_sc-1w4ts_name").text, "1w4ts")
            compare(find(ws, "progressRow_sc-1w4ts_personalBestDate").text, "2026-08-14")
            compare(find(ws, "progressRow_sc-1w4ts_playCount").text, "37")
            compare(find(ws, "progressRow_sc-1w4ts_next").text, "Platinum 900")
            const bar = find(ws, "progressRow_sc-1w4ts_bar")
            verify(bar !== null, "every row has a tier bar")
            compare(bar.description, "Gold · 75% of the way to Platinum (812 / 900)")
            compare(bar.Accessible.name, bar.description, "the bar is described for assistive tech")
        }

        function test_everyScenarioAppearsOnceInDefinitionOrder() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const ids = ["sc-popcorn", "sc-1w4ts", "sc-pasu", "sc-smooth"]
            let previousY = -1
            for (const id of ids) {
                const rows = findAllPrefixed(ws, "progressRow_" + id).filter(function (item) {
                    return item.objectName === "progressRow_" + id
                })
                compare(rows.length, 1, id + " appears exactly once")
                const y = rows[0].mapToItem(ws, 0, 0).y
                verify(y > previousY, id + " follows the previous row")
                previousY = y
            }
        }

        function test_groupCellsSpanExactlyTheirRows() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const spans = [
                {group: "progressGroup_uncategorized", first: "sc-popcorn", last: "sc-popcorn"},
                {group: "progressGroup_cat-click", first: "sc-1w4ts", last: "sc-pasu"},
                {group: "progressGroup_sub-static", first: "sc-1w4ts", last: "sc-1w4ts"},
                {group: "progressGroup_sub-dynamic", first: "sc-pasu", last: "sc-pasu"},
                {group: "progressGroup_cat-track", first: "sc-smooth", last: "sc-smooth"}
            ]
            for (const span of spans) {
                const cell = find(ws, span.group)
                verify(cell !== null && cell.visible, span.group + " missing")
                const first = find(ws, "progressRow_" + span.first)
                const last = find(ws, "progressRow_" + span.last)
                fuzzyCompare(cell.mapToItem(ws, 0, 0).y, first.mapToItem(ws, 0, 0).y, 0.5, span.group + " top")
                fuzzyCompare(cell.mapToItem(ws, 0, cell.height).y, last.mapToItem(ws, 0, last.height).y, 0.5,
                             span.group + " bottom")
            }
            verify(find(ws, "progressGroup_cat-click_name").font.bold, "group names are bold, as in the editor")
            compare(find(ws, "progressHeader_subcategory").visible, true,
                    "a benchmark with subcategories shows the Subcategory column")
        }

        // A category group with no subcategory groups among its rows covers the
        // Subcategory column too; one that has subcategories keeps its own width.
        function test_categoryWithoutSubcategoriesSpansTheSubcategoryColumn() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const category = find(ws, "progressHeader_category")
            const subcategory = find(ws, "progressHeader_subcategory")
            const flat = find(ws, "progressGroup_cat-track")
            const split = find(ws, "progressGroup_cat-click")
            fuzzyCompare(flat.x, category.x, 0.5)
            fuzzyCompare(flat.width, category.width + subcategory.width, 0.5, "Tracking covers both columns")
            fuzzyCompare(find(ws, "progressGroup_uncategorized").width, category.width + subcategory.width, 0.5,
                         "Uncategorized covers both columns")
            fuzzyCompare(split.width, category.width, 0.5, "Clicking keeps its own column")
            fuzzyCompare(find(ws, "progressGroup_sub-static").width, subcategory.width, 0.5)
        }

        // each group cell shows its rank as a chip in that tier's colour.
        function test_groupCellsShowTheirRankChip() {
            const fx = TestDoubles.trackingFixtureTrackable()
            const groups = fx.progressGrid.groups
            groups[3].rankText = "Gold"        // cat-click
            groups[3].rankColor = "#ffc828"
            groups[3].rankBarColor = "#ffc828"
            groups[4].rankText = "Silver"      // cat-track
            groups[4].rankColor = "#c0c0c8"
            groups[4].rankBarColor = "#c0c0c8"
            groups[1].rankText = "Unranked"    // sub-static
            const ws = makeWorkspace(fx)
            wait(0)

            const gold = find(ws, "progressGroup_cat-click_rank")
            const silver = find(ws, "progressGroup_cat-track_rank")
            verify(gold.visible && silver.visible, "ranked groups show a chip")
            verify(Qt.colorEqual(gold.border.color, "#ffc828") && Qt.colorEqual(silver.border.color, "#c0c0c8"),
                   "each chip uses its tier's colour")
            verify(!Qt.colorEqual(gold.color, silver.color), "different ranks look different")
            compare(gold.children[0].text, "Gold")
            const unranked = find(ws, "progressGroup_sub-static_rank")
            verify(unranked.visible && Qt.colorEqual(unranked.border.color, ws.palette.mid),
                   "an unranked group gets a neutral chip")
            const none = find(ws, "progressGroup_uncategorized_rank")
            verify(none === null || !none.visible, "no rank, no chip")

            const cell = find(ws, "progressGroup_cat-click")
            verify(gold.mapToItem(cell, gold.width, 0).x <= cell.width, "the chip fits inside its group cell")
        }

        function test_noSubcategoryColumnWithoutSubcategories() {
            const fx = TestDoubles.trackingFixtureTrackable()
            fx.progressGrid.hasSubcategories = false
            fx.progressGrid.groups = fx.progressGrid.groups.filter(function (g) { return g.column === "category" })
            const ws = makeWorkspace(fx)
            compare(find(ws, "progressHeader_subcategory").visible, false)
            fuzzyCompare(find(ws, "progressHeader_scenario").x,
                         find(ws, "progressHeader_category").x + find(ws, "progressHeader_category").width, 0.5,
                         "the Scenario column follows Category directly")
        }

        // -- responsive layout -----------------------------------------

        function test_wideLayoutPutsHistoriesSideBySideWithAlignedMetricColumns() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 1200, 800)
            const rank = find(ws, "rankHistoryCard")
            const playtime = find(ws, "playtimeHistoryCard")
            verify(rank !== null && playtime !== null, "both history cards must exist")
            const rankPos = rank.mapToItem(ws, 0, 0)
            const playtimePos = playtime.mapToItem(ws, 0, 0)
            verify(Math.abs(rankPos.y - playtimePos.y) < 4 && playtimePos.x > rankPos.x + 10,
                   "at >=1000px content width the two history cards sit side by side")
            const grid = find(ws, "benchmarkProgressGrid")
            // The offscreen test font is wide, so whether the grid fits is the
            // driver's to check; here the scroll bar shows exactly when it overflows.
            compare(find(ws, "progressGridScrollBar").visible, grid.overflowing)
            verify(grid.columns.progress.width >= 4 * grid._measured.chunkMinimum + 16,
                   "the bar column never narrows below one minimum chunk per tier")
        }

        function test_narrowLayoutStacksHistoriesAndScrollsTheGridInItsOwnFrame() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 800, 600)
            const rank = find(ws, "rankHistoryCard")
            const playtime = find(ws, "playtimeHistoryCard")
            verify(rank !== null && playtime !== null, "both history cards must exist")
            const rankPos = rank.mapToItem(ws, 0, 0)
            const playtimePos = playtime.mapToItem(ws, 0, 0)
            verify(Math.abs(rankPos.x - playtimePos.x) < 4 && playtimePos.y > rankPos.y + 10,
                   "below 1000px content width the history cards stack")

            // The grid never pushes the page sideways: it fits, or it scrolls inside
            // its own frame with a visible scroll bar.
            const grid = find(ws, "benchmarkProgressGrid")
            verify(grid.mapToItem(ws, grid.width, 0).x <= ws.width + 1, "the grid's frame fits the width")
            const flick = find(ws, "progressGridFlick")
            if (grid.overflowing) {
                const bar = find(ws, "progressGridScrollBar")
                verify(bar !== null && bar.visible, "an overflowing grid shows its horizontal scroll bar")
                verify(flick.contentWidth > flick.width, "the grid scrolls inside its own frame")
            } else {
                verify(flick.contentWidth <= flick.width + 0.5, "a grid that fits does not scroll")
            }
            // No tier chunk is narrower than its header's name.
            for (let i = 0; i < 4; ++i) {
                const header = find(ws, "progressTierHeader_" + i)
                const name = find(ws, "progressTierHeader_" + i + "_name")
                verify(header.width >= name.implicitWidth, "tier " + i + " header fits its name: "
                       + header.width + " < " + name.implicitWidth)
                const nameLeft = name.mapToItem(header, 0, 0).x
                verify(nameLeft >= 0 && nameLeft + name.implicitWidth <= header.width,
                       "tier " + i + " name lies inside its header")
            }
            verify(find(ws, "progressRow_sc-1w4ts_personalBestDate") !== null
                   && find(ws, "progressRow_sc-1w4ts_playCount") !== null
                   && find(ws, "progressRow_sc-1w4ts_next") !== null,
                   "narrow layout preserves every value")
        }

        // -- horizontal scroll: the right end first, identifying columns pinned --

        // A standalone grid over the trackable fixture. `fit` sizes it to fit; otherwise
        // it gets `scrolling` px beside its pinned columns, so it overflows and pins.
        function makeGrid(fit, scrolling) {
            const data = TestDoubles.trackingFixtureTrackable().progressGrid
            const probe = createTemporaryObject(progressGridComponent, root, {grid: data, width: 4000})
            const fitting = probe.implicitWidth
            const pinned = probe.pinnedWidth
            probe.destroy()
            const width = fit ? fitting + 200 : pinned + (scrolling || 220)
            const grid = createTemporaryObject(progressGridComponent, root,
                                               {grid: data, width: width, benchmarkKey: "Voltaic S3"})
            verify(grid !== null, "the grid failed to instantiate")
            wait(0)
            if (!fit) {
                verify(grid.overflowing, "the grid overflows at " + width + " px")
                verify(grid.pinning, "the grid pins its identifying columns at " + width + " px")
            }
            return grid
        }
        function gridFlick(grid) { return ItemLookup.findByObjectName(grid, "progressGridFlick") }
        // A real drag on the bar area, to the right: the content moves back toward its start.
        function userDrag(grid, dx) {
            const flick = gridFlick(grid)
            mouseDrag(flick, flick.width - 40, grid.headerHeight + grid.rowHeight / 2, dx, 0, Qt.LeftButton,
                      Qt.NoModifier, 20)
            tryCompare(flick, "moving", false, 5000)
        }

        function test_overflowingGridOpensAtItsRightEnd() {
            // Room for Next tier (wide in the test font) and the top tier beside it.
            const grid = makeGrid(false, 420)
            const flick = gridFlick(grid)
            verify(grid.maximumContentX > 0)
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5, "opens scrolled to the right end")
            compare(grid.userScrolled, false)
            const next = ItemLookup.findByObjectName(grid, "progressHeader_next")
            fuzzyCompare(next.mapToItem(grid, next.width, 0).x, grid.width, 0.5, "Next tier is the last thing visible")
            const lastTier = ItemLookup.findByObjectName(grid, "progressTierHeader_" + (grid.tiers.length - 1))
            const lastRight = lastTier.mapToItem(grid, lastTier.width, 0).x
            verify(lastRight <= grid.width - next.width + 0.5 && lastRight > grid.pinnedWidth,
                   "the top tier is in view, beside Next tier and clear of the pinned columns")
        }

        function test_gridThatFitsDoesNotScrollOrPin() {
            const grid = makeGrid(true)
            verify(!grid.overflowing && !grid.pinning)
            compare(gridFlick(grid).contentX, 0)
            compare(grid.pinOffset, 0)
            const edge = ItemLookup.findByObjectName(grid, "progressGridPinEdge")
            verify(!edge.visible, "no pinned edge on a grid that fits")
            fuzzyCompare(ItemLookup.findByObjectName(grid, "progressHeader_scenario").mapToItem(grid, 0, 0).x,
                         grid.columns.scenario.x, 0.5)
        }

        function test_identifyingColumnsStayPinnedWhileTheRestScrolls() {
            const grid = makeGrid(false)
            const flick = gridFlick(grid)
            verify(flick.contentX > 0)
            for (const key of ["category", "subcategory", "scenario"]) {
                const header = ItemLookup.findByObjectName(grid, "progressHeader_" + key)
                fuzzyCompare(header.mapToItem(grid, 0, 0).x, grid.columns[key].x, 0.5, key + " header stays put")
                verify(header.z > 0, key + " header draws over the scrolled tier headers")
            }
            const pbDate = ItemLookup.findByObjectName(grid, "progressHeader_pbDate")
            fuzzyCompare(pbDate.mapToItem(grid, 0, 0).x, grid.columns.pbDate.x - flick.contentX, 0.5,
                         "PB date scrolls under the pinned columns")
            for (const id of ["sc-popcorn", "sc-1w4ts", "sc-pasu", "sc-smooth"]) {
                const name = ItemLookup.findByObjectName(grid, "progressRow_" + id + "_name")
                fuzzyCompare(name.mapToItem(grid, 0, 0).x,
                             grid.columns.scenario.x + grid.cellPadding + grid.markerSpace, 0.5, id + " name stays put")
            }
            const blocker = ItemLookup.findByObjectName(grid, "progressRow_sc-1w4ts_blocker")
            fuzzyCompare(blocker.mapToItem(grid, 0, 0).x, grid.columns.scenario.x + grid.cellPadding - 4, 0.5,
                         "the blocker marker stays beside its name")
            // Group cells, including categories that span the Subcategory column.
            const flat = ItemLookup.findByObjectName(grid, "progressGroup_cat-track")
            const split = ItemLookup.findByObjectName(grid, "progressGroup_cat-click")
            const sub = ItemLookup.findByObjectName(grid, "progressGroup_sub-static")
            fuzzyCompare(flat.mapToItem(grid, 0, 0).x, grid.columns.category.x, 0.5)
            fuzzyCompare(flat.width, grid.columns.category.width + grid.columns.subcategory.width, 0.5,
                         "a spanning category still covers both columns")
            fuzzyCompare(split.mapToItem(grid, 0, 0).x, grid.columns.category.x, 0.5)
            fuzzyCompare(sub.mapToItem(grid, 0, 0).x, grid.columns.subcategory.x, 0.5)
            verify(ItemLookup.findByObjectName(grid, "progressGridPinEdge").visible,
                   "an edge marks where the columns pass under")
        }

        function test_resizeFollowsTheRightEndUntilTheUserScrolls() {
            const grid = makeGrid(false, 300)
            const flick = gridFlick(grid)
            grid.width -= 60
            wait(0)
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5, "a narrower grid still shows its right end")

            userDrag(grid, 120)
            verify(grid.userScrolled, "a drag away from the right end is the user's position")
            const kept = flick.contentX
            verify(kept < grid.maximumContentX - 0.5)
            grid.width -= 40
            wait(0)
            fuzzyCompare(flick.contentX, Math.min(kept, grid.maximumContentX), 0.5, "a resize keeps the user's position")

            userDrag(grid, -2000)
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5)
            verify(!grid.userScrolled, "back at the right end, the grid follows it again")
            grid.width -= 40
            wait(0)
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5)
        }

        function test_benchmarkChangeReturnsToTheRightEnd() {
            const grid = makeGrid(false, 300)
            const flick = gridFlick(grid)
            userDrag(grid, 120)
            verify(grid.userScrolled)
            grid.benchmarkKey = "Revo Novice"
            wait(0)
            verify(!grid.userScrolled, "a new benchmark forgets the user's position")
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5, "and opens at its right end")
        }

        function test_fittingAgainForgetsTheUserPosition() {
            const grid = makeGrid(false, 300)
            const flick = gridFlick(grid)
            userDrag(grid, 120)
            verify(grid.userScrolled)
            grid.width = grid.implicitWidth + 4000
            wait(0)
            compare(flick.contentX, 0, "a grid that fits sits at its start")
            grid.width = grid.pinnedWidth + 260
            wait(0)
            fuzzyCompare(flick.contentX, grid.maximumContentX, 0.5, "overflowing again opens at the right end")
        }

        function test_noOverlapOrClippingAtBothSupportedWindowSizes_data() {
            return [
                {tag: "1200x800", w: 1200, h: 800},
                {tag: "800x600", w: 800, h: 600}
            ]
        }

        function test_noOverlapOrClippingAtBothSupportedWindowSizes(data) {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), data.w, data.h)
            const summary = find(ws, "benchmarkStatusSummary")
            const grid = find(ws, "benchmarkProgressGrid")
            const rank = find(ws, "rankHistoryCard")
            verify(summary !== null && grid !== null, "summary and progress grid must be present")
            wait(0)

            const summaryBottom = summary.mapToItem(ws, 0, summary.height).y
            const gridTop = grid.mapToItem(ws, 0, 0).y
            verify(gridTop >= summaryBottom - 1, "summary and grid must not overlap at " + data.tag)
            verify(rank.mapToItem(ws, 0, 0).y >= grid.mapToItem(ws, 0, grid.height).y - 1,
                   "grid and history cards must not overlap at " + data.tag)

            const col = find(ws, "trackableScrollColumn")
            verify(col !== null && col.width <= ws.width + 1, "content must fit the width at " + data.tag)
        }

        function test_layoutSurvives150PercentFontScale() {
            const host = createTemporaryObject(largeFontHostComponent, root, {})
            verify(host !== null, "large-font host failed to instantiate")

            const ws = instantiateWorkspace(host)
            ws.trackingVm = TestDoubles.makeFakeTrackingVm(TestDoubles.trackingFixtureTrackable())
            ws.width = host.width
            ws.height = host.height
            wait(0)

            const summary = ItemLookup.findByObjectName(ws, "benchmarkStatusSummary")
            const grid = ItemLookup.findByObjectName(ws, "benchmarkProgressGrid")
            verify(summary !== null && grid !== null,
                   "required metrics must remain present at 150% font scale")
            const summaryBottom = summary.mapToItem(host, 0, summary.height).y
            const gridTop = grid.mapToItem(host, 0, 0).y
            verify(gridTop >= summaryBottom - 1, "150% font scale must not overlap controls")
            const name = ItemLookup.findByObjectName(ws, "progressTierHeader_3_name")
            const header = ItemLookup.findByObjectName(ws, "progressTierHeader_3")
            verify(header.width >= name.implicitWidth, "tier headers grow with the font: "
                   + header.width + " < " + name.implicitWidth)
        }

        // -- values share columns across groups, under headers -------

        function test_gridValuesLineUpUnderHeadersAcrossGroups_data() {
            return [
                {tag: "1200x800", w: 1200, h: 800},
                {tag: "800x600", w: 800, h: 600}
            ]
        }

        function test_gridValuesLineUpUnderHeadersAcrossGroups(data) {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), data.w, data.h)
            wait(0)
            const columns = [
                {header: "progressHeader_pbDate", cell: "personalBestDate"},
                {header: "progressHeader_plays", cell: "playCount"},
                {header: "progressHeader_next", cell: "next"}
            ]
            for (const column of columns) {
                const header = find(ws, column.header)
                verify(header !== null && header.visible && String(header.text).length > 0,
                       column.header + " must be a visible heading")
                const headerRight = header.mapToItem(ws, header.width, 0).x
                // Rows under different groups: Uncategorized, Clicking / Static, Tracking.
                for (const id of ["sc-1w4ts", "sc-popcorn", "sc-smooth"]) {
                    const cell = find(ws, "progressRow_" + id + "_" + column.cell)
                    verify(cell !== null, id + " has no " + column.cell + " cell")
                    // Values are right-aligned inside the column's padding.
                    fuzzyCompare(cell.mapToItem(ws, cell.width, 0).x, headerRight - 8, 0.5,
                                 id + " " + column.cell + " must sit under its heading")
                }
            }
            // The first tier header also covers the bar's inset.
            const barInset = find(ws, "benchmarkProgressGrid").barInset
            for (const id of ["sc-1w4ts", "sc-popcorn", "sc-smooth"]) {
                fuzzyCompare(find(ws, "progressRow_" + id + "_bar").mapToItem(ws, 0, 0).x,
                             find(ws, "progressTierHeader_0").mapToItem(ws, 0, 0).x + barInset, 0.5,
                             id + " bar starts under the first tier header")
            }
        }

        // -- only abnormal match states are reported ---------------------

        function test_onlyAbnormalMatchStatesAreReported() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const abnormal = find(ws, "progressRow_sc-popcorn_bar")
            verify(String(abnormal.description).indexOf("Several scenarios match") !== -1,
                   "an abnormal state is reported with the bar")
            verify(String(abnormal.description).indexOf("More than one scenario") !== -1,
                   "an abnormal state carries guidance on what to do")
            const normal = find(ws, "progressRow_sc-1w4ts_bar")
            verify(String(normal.description).indexOf("Resolved") === -1, "a normal match is not reported")
        }

        // -- tier bar edge states ------------------------------------------

        function test_tierBarEdgeStatesEachHaveTheirOwnLook() {
            const fx = TestDoubles.trackingFixtureTrackable()
            const rows = fx.progressGrid.rows
            rows[0].edgeState = "unmatched"                  // sc-popcorn: several scenarios match
            rows[0].statusText = "Several scenarios match"
            rows[2].edgeState = "aboveTop"                   // sc-pasu: above Platinum
            rows[2].aboveTopText = "Above Platinum: 1,040.25 / 1,000"
            rows[3].missingChunks = [false, false, false, true]  // sc-smooth: no Platinum threshold
            rows.push(TestDoubles.makeFakeProgressRow({
                entryId: "sc-new", name: "Brand new", fills: [0, 0, 0, 0],
                edgeState: "unplayed", statusText: "Unplayed"
            }))
            fx.progressGrid.groups[4].span = 2
            const ws = makeWorkspace(fx)
            wait(0)

            const unmatched = find(ws, "progressRow_sc-popcorn_bar")
            verify(find(unmatched, unmatched.objectName + "_outline").visible, "unmatched: outlined bar")
            compare(find(unmatched, unmatched.objectName + "_status").text, "Several scenarios match")
            verify(find(unmatched, unmatched.objectName + "_status").visible, "with its status in the bar")

            const unplayed = find(ws, "progressRow_sc-new_bar")
            const unplayedStatus = find(unplayed, unplayed.objectName + "_status")
            verify(unplayedStatus.visible && unplayedStatus.text === "Unplayed", "unplayed: a muted label")
            verify(!find(unplayed, unplayed.objectName + "_outline").visible, "unplayed is not outlined")
            for (let i = 0; i < 4; ++i)
                verify(!find(unplayed, unplayed.objectName + "_fill_" + i).visible, "unplayed has no fill")

            const above = find(ws, "progressRow_sc-pasu_bar")
            const mark = find(above, above.objectName + "_aboveTop")
            verify(mark.visible, "above the top tier: a mark past the bar's end")
            verify(mark.mapToItem(ws, mark.width, 0).x > above.mapToItem(ws, above.width, 0).x,
                   "the mark sits at the bar's end")
            compare(mark.ToolTip.text, "Above Platinum: 1,040.25 / 1,000")
            const chevron = find(mark, above.objectName + "_aboveTopChevron")
            verify(chevron && chevron.height > chevron.width, "the mark is a chevron, not a diamond")
            const normal = find(ws, "progressRow_sc-1w4ts_bar")
            verify(!find(normal, normal.objectName + "_aboveTop").visible, "only above the top tier")

            const smooth = find(ws, "progressRow_sc-smooth_bar")
            verify(find(smooth, smooth.objectName + "_hatch_3").visible, "a chunk without thresholds is hatched")
            verify(!find(smooth, smooth.objectName + "_hatch_2").visible, "the others are not")
        }

        // -- the score that reaches each tier, at the end of its chunk ----------

        function test_eachChunkShowsTheScoreThatCompletesItAtItsEnd() {
            const fx = TestDoubles.trackingFixtureTrackable()
            const rows = fx.progressGrid.rows
            const scores = ["300", "500", "700", "900"]
            rows[0].thresholdTexts = scores                    // sc-popcorn: unplayed below
            rows[0].edgeState = "unplayed"
            rows[0].statusText = "Unplayed"
            rows[1].thresholdTexts = scores                    // sc-1w4ts: every chunk
            rows[1].personalBestText = "—"                     // (no PB label in the way)
            rows[3].thresholdTexts = scores                   // sc-smooth: Platinum hatched
            rows[3].missingChunks = [false, false, false, true]
            rows[3].personalBestText = "—"
            const ws = makeWorkspace(fx)
            wait(0)

            const bar = find(ws, "progressRow_sc-1w4ts_bar")
            for (let i = 0; i < 4; ++i) {
                const label = find(bar, bar.objectName + "_label_" + i)
                const chunk = find(bar, bar.objectName + "_chunk_" + i)
                verify(label !== null && label.visible, "chunk " + i + " shows its score")
                compare(label.text, scores[i])
                const textRight = label.mapToItem(bar, label.width, 0).x
                const chunkRight = chunk.mapToItem(bar, chunk.width, 0).x
                verify(textRight <= chunkRight && chunkRight - textRight <= 4,
                       "chunk " + i + " label ends at its chunk's right edge")
                compare(label.horizontalAlignment, Text.AlignRight)
            }

            const smooth = find(ws, "progressRow_sc-smooth_bar")
            verify(find(smooth, smooth.objectName + "_label_2").visible, "a usable chunk keeps its score")
            verify(!find(smooth, smooth.objectName + "_label_3").visible, "a hatched chunk shows none")

            const unplayed = find(ws, "progressRow_sc-popcorn_bar")
            for (let i = 0; i < 4; ++i)
                verify(!find(unplayed, unplayed.objectName + "_label_" + i).visible,
                       "the status text owns an unplayed bar's middle")

            const bare = find(ws, "progressRow_sc-pasu_bar")
            for (let i = 0; i < 4; ++i)
                verify(!find(bare, bare.objectName + "_label_" + i).visible, "a row without scores shows none")
        }

        // -- the PB where its fill ends ----------------------------------------------

        function test_personalBestSitsWhereTheFillEnds() {
            const fx = TestDoubles.trackingFixtureTrackable()
            const rows = fx.progressGrid.rows
            rows[0].personalBestText = "250"                   // sc-popcorn: unplayed
            rows[0].edgeState = "unplayed"
            rows[0].statusText = "Unplayed"
            rows[3].thresholdTexts = ["300", "3,000", "3,400", "4,000"]  // sc-smooth: PB 20% into chunk 2
            rows[1].thresholdTexts = ["300", "500", "700", "900"]          // sc-1w4ts: PB near chunk 3's end
            rows[1].fills = [1, 1, 1, 0.9]
            const ws = makeWorkspace(fx)
            wait(0)

            const smooth = find(ws, "progressRow_sc-smooth_bar")
            const value = find(smooth, smooth.objectName + "_value")
            const fill = find(smooth, smooth.objectName + "_fill_2")
            verify(value.visible, "the PB shows on its bar")
            compare(value.text, "3,120")
            verify(Qt.colorEqual(value.color, "#c0c0c8"), "in the attained tier's colour")
            const fillEnd = fill.mapToItem(smooth, fill.width, 0).x
            const valueLeft = value.mapToItem(smooth, 0, 0).x
            verify(valueLeft > fillEnd && valueLeft - fillEnd < 8, "just past the fill's end")
            verify(find(smooth, smooth.objectName + "_label_1").visible, "a tier score clear of the PB stays")

            // A PB near its chunk's end would cover that chunk's score: the score moves out of
            // its way, and the PB stays where the fill ends.
            const near = find(ws, "progressRow_sc-1w4ts_bar")
            const nearValue = find(near, near.objectName + "_value")
            verify(nearValue.visible)
            const moved = find(near, near.objectName + "_label_3")
            const movedChunk = find(near, near.objectName + "_chunk_3")
            verify(moved.visible, "the tier score under the PB stays visible")
            verify(movedChunk.labelPlacement === "middle" || movedChunk.labelPlacement === "start",
                   "it moved off the chunk's end: " + movedChunk.labelPlacement)
            verify(!near.coversValue(3, moved.implicitWidth, movedChunk.labelPlacement), "clear of the PB")
            compare(find(near, near.objectName + "_chunk_2").labelPlacement, "end", "the others stay at their ends")
            verify(find(near, near.objectName + "_label_2").visible, "the others stay")

            // A full bar leaves no room past the fill: the PB moves inside its end.
            const pasu = find(ws, "progressRow_sc-pasu_bar")
            const inside = find(pasu, pasu.objectName + "_value")
            verify(inside.visible)
            verify(inside.mapToItem(pasu, inside.width, 0).x <= pasu.width, "stays on the bar")
            verify(!Qt.colorEqual(inside.color, "#50dcdc"), "readable on the fill rather than in its own colour")

            const unplayed = find(ws, "progressRow_sc-popcorn_bar")
            verify(!find(unplayed, unplayed.objectName + "_value").visible, "the status text owns an unplayed bar")
        }

        // -- a threshold label moves out of the PB's way ------------------------------

        function makeTierBar(props) {
            const bar = createTemporaryObject(tierBarComponent, root, {})
            verify(bar !== null)
            bar.tiers = [{name: "Iron", barColor: "#80808a"}, {name: "Bronze", barColor: "#cd7f32"}]
            bar.labels = ["300", "600"]
            for (const key in props) bar[key] = props[key]
            wait(0)
            return bar
        }

        function placement(bar, index) {
            return find(bar, "probe_chunk_" + index).labelPlacement
        }

        function test_thresholdLabelMovesOutOfThePersonalBestsWay() {
            // Chunk 0 spans [0, 99] of a 200 px bar.
            const clear = makeTierBar({width: 200, fills: [0.2, 0], valueText: "250"})
            compare(placement(clear, 0), "end", "a label clear of the PB stays at its chunk's end")
            compare(find(clear, "probe_label_0").horizontalAlignment, Text.AlignRight)

            const nearEnd = makeTierBar({width: 200, fills: [0.85, 0], valueText: "250"})
            compare(placement(nearEnd, 0), "middle", "a PB at the chunk's end moves the label to the middle")
            verify(find(nearEnd, "probe_label_0").visible)
            compare(find(nearEnd, "probe_label_0").horizontalAlignment, Text.AlignHCenter)
            compare(placement(nearEnd, 1), "end", "the next chunk's label is unaffected")

            const wide = makeTierBar({width: 200, fills: [0.5, 0], valueText: "1,234,567,890"})
            verify(!wide.valueInside, "the PB sits past the fill, not inside it")
            compare(placement(wide, 0), "start", "a PB over the end and middle moves the label to the start")
            verify(find(wide, "probe_label_0").visible)
            compare(find(wide, "probe_label_0").horizontalAlignment, Text.AlignLeft)
            const valueLeft = find(wide, "probe_value").mapToItem(wide, 0, 0).x
            verify(valueLeft > wide.fillEnd && valueLeft - wide.fillEnd < 8, "the PB stays past the fill's end")

            const narrow = makeTierBar({width: 120, fills: [0.3, 0], valueText: "250"})
            verify(!narrow.valueInside, "the PB sits past the fill, not inside it")
            compare(placement(narrow, 0), "", "a narrow chunk the PB covers everywhere hides its label")
            verify(!find(narrow, "probe_label_0").visible)
            verify(find(narrow, "probe_value").visible, "the PB still shows")

            // Of the moved places, one clear of the recent-average marker wins.
            const marked = makeTierBar({width: 200, fills: [0.85, 0], valueText: "250", marker: 0.5})
            compare(placement(marked, 0), "start", "the middle is under the marker, so the start")
        }

        // -- a threshold label moves off the recent-average marker -------------------

        function test_thresholdLabelMovesOffTheMarker() {
            // Chunk 0 spans [0, 99] of a 200 px bar.
            const markedEnd = makeTierBar({width: 200, fills: [0.2, 0], marker: 0.95})
            compare(placement(markedEnd, 0), "middle", "the marker over the chunk's end moves the label to the middle")
            const w = find(markedEnd, "probe_label_0").implicitWidth
            verify(!markedEnd.coversMarker(0, w, "middle"), "clear of the marker there")
            compare(placement(markedEnd, 1), "end", "a chunk the marker is not in keeps its label at the end")

            const markedBoth = makeTierBar({width: 200, fills: [0.2, 0], valueText: "250", marker: 0.5})
            compare(placement(markedBoth, 0), "end", "a marker away from the end leaves the label there")

            // A target label keeps its outline, not just its text, clear of the marker.
            const probe = makeTierBar({width: 400, fills: [0, 0]})
            const textLeft = probe.labelSpanLeft(0, find(probe, "probe_label_0").implicitWidth, "end")
            // 5 px left of the text: clear of the text, not of a target's outline.
            const nearText = (textLeft - 5) / probe.chunkRight(0)
            const plain = makeTierBar({width: 400, fills: [0, 0], marker: nearText})
            compare(placement(plain, 0), "end", "a plain label 5 px from the marker stays put")
            const target = makeTierBar({width: 400, fills: [0.97, 0], marker: nearText})
            compare(placement(target, 0), "middle", "a target's outline would touch the marker, so it moves")
            verify(isTarget(target, 0), "and stays a target in the fill")
        }

        function test_thresholdLabelStaysUnderTheMarkerOnlyWhenNothingElseIsClear() {
            // The PB covers the end and middle; the marker the start: avoiding the PB wins.
            const blocked = makeTierBar({width: 200, fills: [0.5, 0], valueText: "1,234,567,890", marker: 0.05})
            const w = find(blocked, "probe_label_0").implicitWidth
            verify(blocked.coversMarker(0, w, "start"), "the marker is over the start")
            compare(placement(blocked, 0), "start", "under the marker rather than under the PB")
            verify(find(blocked, "probe_label_0").visible)

            // A chunk so narrow the marker reaches every place: the label stays at its end.
            const narrow = makeTierBar({width: 60, fills: [0, 0], marker: 0.5})
            const nw = find(narrow, "probe_label_0").implicitWidth
            verify(["end", "middle", "start"].every(p => narrow.coversMarker(0, nw, p)), "no place is clear")
            compare(placement(narrow, 0), "end")
            verify(find(narrow, "probe_label_0").visible)
        }

        // -- the marker is cut behind the PB it crosses ----------------------------------

        function test_markerCrossingThePersonalBestIsCutBehindIt() {
            // The PB "250" sits just past a fill ending at 0.3 of chunk 0 ([0, 99]).
            const crossed = makeTierBar({width: 200, fills: [0.3, 0], valueText: "250", marker: 0.4})
            const value = find(crossed, "probe_value")
            const x = crossed.scaleX(0.4)
            verify(x > value.x && x < value.x + value.width, "the marker passes through the PB text")
            verify(crossed.markerCrossesValue)
            const top = find(crossed, "probe_markerTick")
            const bottom = find(crossed, "probe_markerTickBottom")
            verify(bottom.visible, "the tick's lower end shows")
            const marker = find(crossed, "probe_marker")
            verify(top.mapToItem(crossed, 0, top.height).y < crossed.height / 2 - 3,
                   "the upper end stops above the text")
            verify(bottom.mapToItem(crossed, 0, 0).y > crossed.height / 2 + 3, "the lower end starts below it")
            verify(top.mapToItem(crossed, 0, 0).y < 0 && bottom.mapToItem(crossed, 0, bottom.height).y > crossed.height,
                   "both ends still overhang the bar")
            compare(value.x, crossed.valueLeft, "the PB does not move")

            const clear = makeTierBar({width: 200, fills: [0.3, 0], valueText: "250", marker: 0.1})
            verify(!clear.markerCrossesValue)
            verify(!find(clear, "probe_markerTickBottom").visible, "a marker clear of the PB is one whole tick")
            compare(find(clear, "probe_markerTick").height, find(clear, "probe_marker").height)
        }

        // -- a threshold label inside a partial fill is styled as a target -------------

        function isTarget(bar, index) {
            const chunk = find(bar, "probe_chunk_" + index)
            compare(find(bar, "probe_target_" + index).visible, chunk.labelIsTarget,
                    "the outline shows exactly for a target label")
            return chunk.labelIsTarget
        }

        function test_thresholdLabelInsideAPartialFillIsATarget() {
            // Chunk 0 spans [0, 99] of a 200 px bar; chunk 1 [101, 200].
            const moved = makeTierBar({width: 200, fills: [0.85, 0], valueText: "250"})
            compare(placement(moved, 0), "middle")
            verify(isTarget(moved, 0), "a label moved into the partial fill reads as a target")
            verify(!isTarget(moved, 1), "a label over an empty chunk keeps its look")
            compare(find(moved, "probe_label_0").text, "300", "the text itself is unchanged")

            const start = makeTierBar({width: 200, fills: [0.5, 0], valueText: "1,234,567,890"})
            compare(placement(start, 0), "start")
            verify(isTarget(start, 0), "a label moved to the start of a partial fill reads as a target")

            const clear = makeTierBar({width: 200, fills: [0.2, 0], valueText: "250"})
            compare(placement(clear, 0), "end")
            verify(!isTarget(clear, 0), "a label over the unfilled part keeps its look")

            const full = makeTierBar({width: 200, fills: [1, 0.85], valueText: "650"})
            compare(placement(full, 0), "end")
            verify(!isTarget(full, 0), "a label at the end of a full chunk keeps its look")
            verify(placement(full, 1) === "middle" || placement(full, 1) === "start", placement(full, 1))
            verify(isTarget(full, 1), "the partial chunk's moved label is a target")

            const noValue = makeTierBar({width: 200, fills: [0.97, 0]})
            compare(placement(noValue, 0), "end")
            verify(isTarget(noValue, 0), "a fill reaching under the label at its end makes it a target too")

            const narrow = makeTierBar({width: 120, fills: [0.3, 0], valueText: "250"})
            compare(placement(narrow, 0), "")
            verify(!isTarget(narrow, 0), "a hidden label is never a target")
        }

        // -- rank names use their tier colours --------------------------

        function test_rankNamesUseTierColours() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            verify(Qt.colorEqual(find(ws, "summaryAttainedRank").color, "#ffc828"),
                   "summary attained rank uses its tier colour")
            verify(Qt.colorEqual(find(ws, "summaryCompletedRank").color, "#c0c0c8"),
                   "summary completed rank uses its tier colour")
            verify(Qt.colorEqual(find(ws, "summaryNextTier").color, "#50dcdc"),
                   "summary next tier uses its tier colour")
            const next = find(ws, "progressRow_sc-1w4ts_next")
            verify(Qt.colorEqual(next.children[1].color, "#50dcdc"), "the grid's next tier uses its tier colour")
            verify(!Qt.colorEqual(find(ws, "summaryTotalPlaytime").color, "#ffc828"),
                   "non-tier values keep the normal text colour")
        }

        // -- segmented tier bars ------------------------------------------

        function test_tierBarsShareChunkBoundariesWithTheTierHeaders() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const bars = ["sc-popcorn", "sc-1w4ts", "sc-pasu", "sc-smooth"].map(function (id) {
                return find(ws, "progressRow_" + id + "_bar")
            })
            const grid = find(ws, "benchmarkProgressGrid")
            for (let i = 0; i < 4; ++i) {
                const header = find(ws, "progressTierHeader_" + i)
                // The outer cells also cover the bar's inset; their chunk edge is past it.
                const headerLeft = header.mapToItem(ws, 0, 0).x + (i === 0 ? grid.barInset : 0)
                const headerRight = header.mapToItem(ws, header.width, 0).x - (i === 3 ? grid.barInset : 0)
                for (const bar of bars) {
                    const chunk = find(bar, bar.objectName + "_chunk_" + i)
                    verify(chunk !== null, bar.objectName + " has a chunk per tier")
                    verify(Math.abs(chunk.mapToItem(ws, 0, 0).x - headerLeft) <= 1.5,
                           bar.objectName + " chunk " + i + " starts under its tier header")
                    verify(Math.abs(chunk.mapToItem(ws, chunk.width, 0).x - headerRight) <= 1.5,
                           bar.objectName + " chunk " + i + " ends under its tier header")
                }
                const name = find(ws, "progressTierHeader_" + i + "_name")
                const center = name.mapToItem(ws, name.width / 2, 0).x
                verify(Math.abs(center - (headerLeft + headerRight) / 2) <= 12,
                       "tier " + i + " name is centred over its chunk")
            }
        }

        function test_tierBarFillsEachChunkInItsTierColour() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const bar = find(ws, "progressRow_sc-1w4ts_bar")
            verify(bar.height >= 14, "the bar is tall enough to hover")
            const expected = [1, 1, 1, 0.75]
            const colours = ["#cd7f32", "#c0c0c8", "#ffc828", "#50dcdc"]
            for (let i = 0; i < 4; ++i) {
                const chunk = find(bar, bar.objectName + "_chunk_" + i)
                const fill = find(bar, bar.objectName + "_fill_" + i)
                fuzzyCompare(fill.width, chunk.width * expected[i], 1, "chunk " + i + " fill")
                verify(Qt.colorEqual(fill.color, colours[i]), "chunk " + i + " uses its tier's colour")
            }
            const unplayed = find(ws, "progressRow_sc-popcorn_bar")
            for (let j = 0; j < 4; ++j)
                compare(find(unplayed, unplayed.objectName + "_fill_" + j).visible, false,
                        "an unplayed scenario shows no fill")
            verify(String(find(ws, "nextTierProgress").text).indexOf("block") !== -1, "the heading is still shown")
        }

        // the recent average reads against the PB fill on the same chunk scale.
        function test_recentAverageMarkerSitsOnTheChunkScale() {
            const fx = TestDoubles.trackingFixtureTrackable()
            // sc-1w4ts: PB fill ends at 3.75; recent 2.5 is halfway through Gold's chunk.
            fx.progressGrid.rows[1].recentPosition = 2.5
            fx.progressGrid.rows[1].recentDescription = "Recent average 790.5 · Silver"
            const ws = makeWorkspace(fx)
            wait(0)
            const bar = find(ws, "progressRow_sc-1w4ts_bar")
            const marker = find(ws, "progressRow_sc-1w4ts_bar_marker")
            verify(marker !== null && marker.visible, "a row with a recent value shows the marker")
            const chunk = find(bar, bar.objectName + "_chunk_2")
            const markerCentre = marker.mapToItem(ws, marker.width / 2, 0).x
            fuzzyCompare(markerCentre, chunk.mapToItem(ws, chunk.width / 2, 0).x, 1,
                         "2.5 is the middle of the third chunk")
            const fill = find(bar, bar.objectName + "_fill_3")
            verify(markerCentre < fill.mapToItem(ws, fill.width, 0).x, "below the PB: left of the fill's end")
            verify(marker.height > bar.height, "the tick overhangs the bar so it shows over fill and track")
            compare(marker.ToolTip.text, "Recent average 790.5 · Silver", "the marker has its own tooltip")

            for (const id of ["sc-popcorn", "sc-pasu"]) {
                const none = find(ws, "progressRow_" + id + "_bar_marker")
                verify(none === null || !none.visible, id + " has no recent value and no marker")
            }
        }

        // -- focus order ----------------------------------------------

        function test_visualTabOrderReachesManagerAction() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const selector = find(ws, "benchmarkSelector")
            const manage = find(ws, "manageBenchmarksButton")
            verify(selector !== null && manage !== null, "selector and manager action must both exist")

            selector.forceActiveFocus()
            verify(selector.activeFocus, "benchmark selector must be focusable")

            let hop = selector
            let reachedManage = false
            for (let i = 0; i < 16 && hop !== null; ++i) {
                hop = hop.nextItemInFocusChain(true)
                if (hop === manage) { reachedManage = true; break }
            }
            verify(reachedManage, "Manage Benchmarks must be reachable in forward visual tab order")
        }
    }
}
