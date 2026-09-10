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

        function test_headerIsPresentInEveryState_data() {
            return [
                {tag: "empty", fx: TestDoubles.trackingFixtureEmptyLibrary()},
                {tag: "problems", fx: TestDoubles.trackingFixtureProblemsOnly()},
                {tag: "noselection", fx: TestDoubles.trackingFixtureNoSelection()},
                {tag: "unavailable", fx: TestDoubles.trackingFixtureUnavailable()},
                {tag: "incomplete", fx: TestDoubles.trackingFixtureIncomplete()},
                {tag: "trackable", fx: TestDoubles.trackingFixtureTrackable()}
            ]
        }

        function test_headerIsPresentInEveryState(data) {
            const ws = makeWorkspace(data.fx)
            const selector = find(ws, "benchmarkSelector")
            const classification = find(ws, "benchmarkClassificationLabel")
            const manage = find(ws, "manageBenchmarksButton")
            verify(selector !== null, "header benchmark selector missing")
            verify(classification !== null, "header classification/status label missing")
            verify(manage !== null, "header Manage Benchmarks button missing")
            verify(String(selector.Accessible.name).length > 0, "selector needs an accessible name")
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

        function test_neverAutoSelectsTheOnlyEntry() {
            const only = TestDoubles.trackingFixtureNoSelection()
            only.selectorEntries = [
                {id: "bench-A", name: "Voltaic S3", classification: "Trackable", selectable: true}
            ]
            const ws = makeWorkspace(only)
            compare(ws.trackingVm.selectBenchmarkCalls.length, 0,
                    "a single loaded definition must not be auto-selected")
            verify(find(ws, "noSelectionBody") !== null,
                   "with one entry and no selection the no-selection body must show")
        }

        // -- state bodies ----------------------------------------------------

        function test_emptyLibraryOffersCreateOrImport() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureEmptyLibrary())
            verify(find(ws, "emptyLibraryBody") !== null, "empty-library body missing")
            verify(find(ws, "createOrImportButton") !== null,
                   "empty library must offer 'Create or import a benchmark'")
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
            compare(find(ws, "benchmarkBlockerList"), null, "no stale blocker list in Unavailable")
            compare(find(ws, "rankHistoryCard"), null, "no stale rank graph in Unavailable")
            compare(find(ws, "playtimeHistoryCard"), null, "no stale playtime graph in Unavailable")
            compare(find(ws, "benchmarkBreakdownView"), null, "no stale hierarchy in Unavailable")
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

        function test_trackableBlockersNameScenarioAndGroupWithCurrentVsRequired() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            verify(find(ws, "benchmarkBlockerList") !== null,
                   "trackable state must show the authoritative blocker list")

            const rows = findAllPrefixed(ws, "blocker_")
            compare(rows.length, 3, "every next-tier blocker must be listed")

            const first = find(ws, "blocker_0")
            const firstText = String(first.text) + " "
                + String(first.Accessible ? first.Accessible.name : "")
            verify(firstText.indexOf("1w4ts") !== -1, "scenario blocker must name the scenario")
            verify(firstText.indexOf("Clicking") !== -1 || firstText.indexOf("Static") !== -1,
                   "scenario blocker must name its category/subcategory")
            verify(firstText.indexOf("812") !== -1, "scenario blocker must show the current PB")
            verify(firstText.indexOf("900") !== -1, "scenario blocker must show the required score")

            verify(String(find(ws, "blocker_1").text).indexOf("Unplayed") !== -1,
                   "an unplayed scenario blocker must say 'Unplayed', not a score")
            verify(String(find(ws, "blocker_2").text).indexOf("Switching") !== -1,
                   "group blocker must name the group")
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
            const breakdown = indexInColumn(col, "benchmarkBreakdownView")
            verify(summary >= 0 && progress >= 0 && rank >= 0 && playtime >= 0 && breakdown >= 0,
                   "all four content blocks must be present in the scroll column")
            verify(summary < progress, "status summary comes before next-tier progress")
            verify(progress < rank, "next-tier progress/blockers come before the history cards")
            verify(rank < playtime, "average-rank card comes before rolling-playtime card")
            verify(playtime < breakdown, "history cards come before the hierarchical breakdown")
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

            verify(String(rankCard.Accessible.description).toLowerCase().indexOf("rank") !== -1,
                   "rank card description must include its metric name")
            verify(String(playtimeCard.Accessible.description).toLowerCase().indexOf("playtime") !== -1,
                   "playtime card description must include its metric name")
        }

        // -- hierarchy: metrics, labels, expansion ----------------------

        function test_scenarioDelegateExposesEveryMetricWithMatchingLabel() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const node = find(ws, "breakdownNode_sc-1w4ts")
            verify(node !== null, "scenario delegate for sc-1w4ts missing")
            const text = ItemLookup.findByObjectNamePrefix(node, "", []).map(function (i) {
                return i.text === undefined ? "" : String(i.text)
            }).join(" | ")

            verify(text.indexOf("1w4ts") !== -1, "scenario name")
            verify(text.indexOf("812") !== -1, "personal best value")
            verify(text.indexOf("790.5") !== -1, "recent average value")
            verify(text.indexOf("Gold") !== -1, "attained tier")
            verify(text.indexOf("Platinum") !== -1, "next threshold")
            verify(text.indexOf("Resolved") !== -1, "matching-state label")

            verify(find(ws, "breakdownNode_sc-1w4ts_personalBest") !== null
                   && find(ws, "breakdownNode_sc-1w4ts_recentAverage") !== null,
                   "personal best and recent average must be distinct labelled fields")
        }

        function test_topLevelGroupsAndSubcategoriesStartExpanded() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            verify(find(ws, "breakdownNode_sub-static") !== null,
                   "a subcategory under an expanded category must be visible at start")
            verify(find(ws, "breakdownNode_sc-1w4ts") !== null,
                   "a scenario under an expanded subcategory must be visible at start")
        }

        function test_expansionTogglesByMouseAndKeyboardAndSurvivesRefresh() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const toggle = find(ws, "breakdownToggle_cat-click")
            verify(toggle !== null, "category expansion control missing")

            mouseClick(toggle)
            compare(ws.trackingVm.setExpandedCalls.length, 1, "mouse toggle must call setExpanded")
            compare(ws.trackingVm.setExpandedCalls[0][0], "cat-click")

            toggle.forceActiveFocus()
            keyClick(Qt.Key_Space)
            compare(ws.trackingVm.setExpandedCalls.length, 2, "keyboard (Space) must also toggle expansion")

            ws.trackingVm.breakdown.node("cat-click").expanded = false
            ws.trackingVm = ws.trackingVm   // re-assign to simulate a projection refresh notification
            verify(find(ws, "breakdownNode_cat-click") !== null,
                   "the category node must remain after a same-benchmark refresh")
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
            verify(find(ws, "breakdownNode_sc-1w4ts_metricsRow") !== null,
                   "wide layout uses an aligned metric column row")
        }

        function test_narrowLayoutStacksHistoriesAndUsesTwoLineMetricRows() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), 800, 600)
            const rank = find(ws, "rankHistoryCard")
            const playtime = find(ws, "playtimeHistoryCard")
            verify(rank !== null && playtime !== null, "both history cards must exist")
            const rankPos = rank.mapToItem(ws, 0, 0)
            const playtimePos = playtime.mapToItem(ws, 0, 0)
            verify(Math.abs(rankPos.x - playtimePos.x) < 4 && playtimePos.y > rankPos.y + 10,
                   "below 1000px content width the history cards stack")

            verify(find(ws, "breakdownNode_sc-1w4ts_twoLineMetrics") !== null,
                   "narrow layout uses a two-line metric row")
            verify(find(ws, "breakdownNode_sc-1w4ts_personalBest") !== null
                   && find(ws, "breakdownNode_sc-1w4ts_recentAverage") !== null
                   && find(ws, "breakdownNode_sc-1w4ts_attainedTier") !== null,
                   "narrow layout preserves every required metric value")
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
            const breakdown = find(ws, "benchmarkBreakdownView")
            verify(summary !== null && breakdown !== null, "summary and breakdown must be present")

            const summaryBottom = summary.mapToItem(ws, 0, summary.height).y
            const breakdownTop = breakdown.mapToItem(ws, 0, 0).y
            verify(breakdownTop >= summaryBottom - 1,
                   "summary and breakdown must not overlap at " + data.tag)

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
            const breakdown = ItemLookup.findByObjectName(ws, "benchmarkBreakdownView")
            verify(summary !== null && breakdown !== null,
                   "required metrics must remain present at 150% font scale")
            const summaryBottom = summary.mapToItem(host, 0, summary.height).y
            const breakdownTop = breakdown.mapToItem(host, 0, 0).y
            verify(breakdownTop >= summaryBottom - 1, "150% font scale must not overlap controls")
        }

        // -- focus order ----------------------------------------------

        function test_visualTabOrderReachesManagerActionAndExpansionControls() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const selector = find(ws, "benchmarkSelector")
            const manage = find(ws, "manageBenchmarksButton")
            const firstToggle = find(ws, "breakdownToggle_cat-click")
            verify(selector !== null && manage !== null && firstToggle !== null,
                   "selector, manager action and expansion control must all exist")

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
