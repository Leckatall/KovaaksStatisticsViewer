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
            ws.manageBenchmarksRequested.connect(function () { root.managerRequests++ })
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
            fx.completedRankPosition = 2      // Silver, the second tier
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

        function test_nextTierProgressShowsCountButNotAsTheRule() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const progress = find(ws, "nextTierProgress")
            verify(progress !== null, "next-tier progress element missing")
            const t = String(progress.text)
            verify(t.indexOf("3") !== -1 && t.indexOf("9") !== -1,
                   "progress shows scenariosAtNextTier against the total scenario count")
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

        // -- progress grid: rows, values, group cells ---------------------

        function test_scenarioRowExposesEveryValueInItsOwnCell() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const row = find(ws, "progressRow_sc-1w4ts")
            verify(row !== null, "row for sc-1w4ts missing")
            compare(find(ws, "progressRow_sc-1w4ts_name").text, "1w4ts")
            compare(find(ws, "progressRow_sc-1w4ts_personalBest").text, "812")
            compare(find(ws, "progressRow_sc-1w4ts_recent").text, "790.5")
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
            verify(grid.columns.progress.width >= 4 * grid._measured.chunk + 16,
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
                verify(header.width >= name.implicitWidth + 14, "tier " + i + " header fits its name: "
                       + header.width + " < " + name.implicitWidth)
            }
            verify(find(ws, "progressRow_sc-1w4ts_personalBest") !== null
                   && find(ws, "progressRow_sc-1w4ts_recent") !== null
                   && find(ws, "progressRow_sc-1w4ts_next") !== null,
                   "narrow layout preserves every value")
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
                {header: "progressHeader_pb", cell: "personalBest"},
                {header: "progressHeader_recent", cell: "recent"},
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
            for (const id of ["sc-1w4ts", "sc-popcorn", "sc-smooth"]) {
                fuzzyCompare(find(ws, "progressRow_" + id + "_bar").mapToItem(ws, 0, 0).x,
                             find(ws, "progressTierHeader_0").mapToItem(ws, 0, 0).x, 0.5,
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
            const normal = find(ws, "progressRow_sc-1w4ts_bar")
            verify(!find(normal, normal.objectName + "_aboveTop").visible, "only above the top tier")

            const smooth = find(ws, "progressRow_sc-smooth_bar")
            verify(find(smooth, smooth.objectName + "_hatch_3").visible, "a chunk without thresholds is hatched")
            verify(!find(smooth, smooth.objectName + "_hatch_2").visible, "the others are not")
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
            for (let i = 0; i < 4; ++i) {
                const header = find(ws, "progressTierHeader_" + i)
                const headerLeft = header.mapToItem(ws, 0, 0).x
                const headerRight = header.mapToItem(ws, header.width, 0).x
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
            const progress = String(find(ws, "nextTierProgress").text)
            verify(progress.indexOf("3 of 9") !== -1, "the overall count is still shown")
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
