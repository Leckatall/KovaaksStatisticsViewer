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
            verify(text.indexOf("Platinum") !== -1, "next tier")
            verify(text.indexOf("900") !== -1, "next threshold")
            verify(text.indexOf("Resolved") === -1, "a normal match state is not shown")

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

            verify(find(ws, "breakdownNode_sc-1w4ts_metricsRow") !== null,
                   "narrow layout keeps the aligned metric columns")
            const status = find(ws, "breakdownNode_sc-popcorn_matchState")
            verify(status !== null, "status column missing")
            verify(status.mapToItem(ws, status.width, 0).x <= ws.width + 1,
                   "the last column must fit the narrow width: right edge "
                   + status.mapToItem(ws, status.width, 0).x + " > " + ws.width
                   + " (x " + status.x + ", w " + status.width + ") cols "
                   + ["name", "pb", "recent", "rank", "next", "status"].map(function (k) {
                         const h = find(ws, "breakdownHeader_" + k)
                         return k + "=" + h.x + "/" + h.width
                     }).join(" ") + " view " + find(ws, "benchmarkBreakdownView").width)
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

        // -- values share columns across groups, under headers -------

        function test_breakdownValuesLineUpUnderHeadersAcrossGroups_data() {
            return [
                {tag: "1200x800", w: 1200, h: 800},
                {tag: "800x600", w: 800, h: 600}
            ]
        }

        function test_breakdownValuesLineUpUnderHeadersAcrossGroups(data) {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable(), data.w, data.h)
            const columns = [
                {header: "breakdownHeader_pb", cell: "personalBest"},
                {header: "breakdownHeader_recent", cell: "recentAverage"},
                {header: "breakdownHeader_rank", cell: "attainedTier"},
                {header: "breakdownHeader_next", cell: "nextThreshold"},
                {header: "breakdownHeader_status", cell: "matchState"}
            ]
            for (const column of columns) {
                const header = find(ws, column.header)
                verify(header !== null && header.visible && String(header.text).length > 0,
                       column.header + " must be a visible heading")
                const headerX = header.mapToItem(ws, 0, 0).x
                // sc-1w4ts sits two levels deep (Clicking / Static); sc-popcorn one
                // level deep in Uncategorized.
                for (const id of ["sc-1w4ts", "sc-popcorn"]) {
                    const cell = find(ws, "breakdownNode_" + id + "_" + column.cell)
                    verify(cell !== null, id + " has no " + column.cell + " cell")
                    fuzzyCompare(cell.mapToItem(ws, 0, 0).x, headerX, 0.5,
                                 id + " " + column.cell + " must sit under its heading")
                    fuzzyCompare(cell.width, header.width, 0.5,
                                 id + " " + column.cell + " must share its column's width")
                }
            }
        }

        // -- only abnormal match states are shown ---------------------

        function test_onlyAbnormalMatchStatesAreShown() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const abnormal = find(ws, "breakdownNode_sc-popcorn_matchState")
            verify(abnormal !== null && abnormal.visible, "the abnormal state cell is missing")
            compare(abnormal.text, "Several scenarios match")
            verify(String(abnormal.Accessible.description).length > 0,
                   "an abnormal state carries guidance on what to do")
            const normal = find(ws, "breakdownNode_sc-1w4ts_matchState")
            verify(normal !== null, "status cell missing")
            verify(String(normal.text).indexOf("Resolved") === -1, "a normal match is not reported")
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
            verify(Qt.colorEqual(find(ws, "breakdownNode_sc-1w4ts_attainedTier").color, "#ffc828"),
                   "breakdown rank uses its tier colour")
            verify(!Qt.colorEqual(find(ws, "summaryAverageRank").color, "#ffc828"),
                   "non-tier values keep the normal text colour")
        }

        // -- next-tier progress is drawn per scenario -------------------

        function test_nextTierProgressIsDrawnPerScenario() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            wait(0)
            const bar0 = find(ws, "blockerBar_0")
            const fill0 = find(ws, "blockerFill_0")
            const bar1 = find(ws, "blockerBar_1")
            const fill1 = find(ws, "blockerFill_1")
            verify(bar0 !== null && fill0 !== null && bar1 !== null && fill1 !== null,
                   "every blocking scenario needs a progress bar")
            verify(bar0.width > 40, "the bar must have a usable length")
            fuzzyCompare(fill0.width, bar0.width * 0.75, 1, "the fill reflects the scenario's progress")
            compare(fill1.visible, false, "an unplayed scenario shows no progress")
            fuzzyCompare(bar0.mapToItem(ws, 0, 0).x, bar1.mapToItem(ws, 0, 0).x, 0.5,
                         "bars start at the same x so they can be compared")
            verify(Qt.colorEqual(fill0.color, "#50dcdc"), "the bar uses the next tier's colour")
            verify(String(find(ws, "blocker_0").Accessible.description).indexOf("75%") !== -1,
                   "the bar's progress is described for assistive tech")
            const progress = String(find(ws, "nextTierProgress").text)
            verify(progress.indexOf("3 of 9") !== -1, "the overall count is still shown")
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
