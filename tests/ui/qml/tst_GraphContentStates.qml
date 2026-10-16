import QtQuick
import QtQuick.Controls
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"
import "../../../src/ui/qml/benchmark"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

// Each graph tells "no history", "nothing in the selected range", "no improvement points" and the
// run graph's own unavailable states apart, and the gear stays within reach in every one of them
// (EDGE-03, EDGE-04, EDGE-06).
Item {
    id: root
    width: 1200
    height: 800

    FakeQuickSettingsVm { id: quickVm }

    QtObject {
        id: fakeGraphVm
        property int contentState: GraphViewModel.NoRunSelected
        property string scenarioTitle: ""
        property var enabledSeriesIds: []
        property var allSeries: []
        property string axisLabelSeriesId: ""
        property bool pointMarkers: false
        property int connectionMode: 0
        property var columnForSeriesId: function (id) { return -1 }
        property var seriesIdForColumn: function (column) { return "" }
    }

    QtObject {
        id: fakeVisualSettings
        property bool controlPanelVisible: true
        property var isSeriesVisible: function (id) { return true }
        property var setSeriesVisible: function (id, visible) {}
    }

    QtObject {
        id: historySettings
        property var value: ({"1": true, "2": false, "3": false, "4": false, "5": false})
        property string yAxisColumnKey: "1"
    }

    QtObject {
        id: fakePlaytimeVm
        property bool hasData: false
        property bool historyAvailable: false
        property bool noObservationsInRange: false
        property string emptyStateText: "No playtime recorded yet."
        property bool pointMarkers: false
        property int connectionMode: 0
        property string title: "Daily Playtime (3-day rolling average)"
    }

    Component {
        id: runPanelComponent
        DashboardGraphCanvas {
            width: 900
            height: 400
            graphVm: fakeGraphVm
            visualSettings: fakeVisualSettings
            settingsVm: quickVm
        }
    }

    Component {
        id: playtimePanelComponent
        PlaytimeGraphPanel {
            width: 900
            height: 400
            playtimeVm: fakePlaytimeVm
            settingsVm: quickVm
        }
    }

    Component {
        id: historyPanelComponent
        ScenarioHistoryPanel {
            width: 900
            height: 400
            columnVisibility: historySettings.value
            historyAxisSettings: historySettings
            settingsVm: quickVm
        }
    }

    Component {
        id: cardComponent
        BenchmarkHistoryCard {
            width: 500
            height: 300
            graphKey: "benchmarkRank"
            namePrefix: "rankHistory"
            settingsVm: quickVm
        }
    }

    Component {
        id: workspaceComponent
        BenchmarkWorkspace {
            width: 1200
            height: 800
        }
    }

    TestCase {
        name: "GraphContentStatesTest"
        when: windowShown

        function init() {
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            fakePlaytimeVm.hasData = false
            fakePlaytimeVm.historyAvailable = false
            fakePlaytimeVm.noObservationsInRange = false
            fakePlaytimeVm.emptyStateText = "No playtime recorded yet."
        }

        function find(item, name) { return ItemLookup.findByObjectName(item, name) }

        function shown(item, name) {
            const found = find(item, name)
            return found !== null && found.visible
        }

        function gearOf(item, key) {
            const gear = find(item, "graphGear_" + key)
            verify(gear !== null, "no graphGear_" + key)
            verify(gear.visible, "graphGear_" + key + " is hidden")
            return gear
        }

        // The gear opens its popup, so it is usable and not merely present.
        function verifyGearOpens(gear) {
            mouseClick(gear)
            tryVerify(() => gear.popup.opened, 2000, "the popup did not open from this state")
            gear.popup.close()
            tryVerify(() => !gear.popup.opened, 2000)
        }

        // ---- Run graph (EDGE-06) ------------------------------------------------------------------------

        function test_runGraphWithoutASelectedRunSaysSoAndKeepsTheGear() {
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            const panel = createTemporaryObject(runPanelComponent, root)
            verify(shown(panel, "graphNoRunSelectedLabel"))
            verify(!shown(panel, "graphNoPerformanceLabel"))
            compare(find(panel, "scenarioGraphCanvas"), null)
            verifyGearOpens(gearOf(panel, "run"))
        }

        function test_runGraphWithoutPerformanceDataSaysSoAndKeepsTheGear() {
            fakeGraphVm.contentState = GraphViewModel.NoPerformanceData
            const panel = createTemporaryObject(runPanelComponent, root)
            verify(shown(panel, "graphNoPerformanceLabel"))
            verify(!shown(panel, "graphNoRunSelectedLabel"))
            compare(find(panel, "scenarioGraphCanvas"), null)
            verifyGearOpens(gearOf(panel, "run"))
        }

        function test_theRunGraphGearSurvivesEveryStateChange() {
            const panel = createTemporaryObject(runPanelComponent, root)
            const gear = gearOf(panel, "run")
            fakeGraphVm.contentState = GraphViewModel.NoPerformanceData
            compare(find(panel, "graphGear_run"), gear)
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            compare(find(panel, "graphGear_run"), gear)
        }

        // ---- Daily playtime (EDGE-03, EDGE-04) -----------------------------------------------------------

        function test_noPlaytimeAtAllIsTheOrdinaryNoHistoryStateWithoutARangeHint() {
            const panel = createTemporaryObject(playtimePanelComponent, root)
            const label = find(panel, "playtimeEmptyLabel")
            verify(label.visible)
            compare(label.text, "No playtime recorded yet.")
            verify(!shown(panel, "playtimeEmptyHint"), "nothing to widen when there is no history")
            verifyGearOpens(gearOf(panel, "dailyPlaytime"))
        }

        function test_anEmptyPlaytimeRangeIsDistinguishedAndPointsToTheGear() {
            fakePlaytimeVm.historyAvailable = true
            fakePlaytimeVm.noObservationsInRange = true
            fakePlaytimeVm.emptyStateText = "No playtime in the selected range."
            const panel = createTemporaryObject(playtimePanelComponent, root)

            compare(find(panel, "playtimeEmptyLabel").text, "No playtime in the selected range.")
            const hint = find(panel, "playtimeEmptyHint")
            verify(hint !== null && hint.visible, "the empty range hints at widening it")
            verify(hint.text.indexOf("range") >= 0)
            compare(find(panel, "playtimeGraphCanvas"), null, "no activity is fabricated to fill the range")
            verifyGearOpens(gearOf(panel, "dailyPlaytime"))
        }

        function test_theRangeHintFollowsTheViewModel() {
            const panel = createTemporaryObject(playtimePanelComponent, root)
            verify(!shown(panel, "playtimeEmptyHint"))
            fakePlaytimeVm.historyAvailable = true
            fakePlaytimeVm.noObservationsInRange = true
            verify(shown(panel, "playtimeEmptyHint"))
            fakePlaytimeVm.hasData = true
            verify(!shown(panel, "playtimeEmptyHint"))
            verify(!shown(panel, "playtimeEmptyLabel"))
        }

        // ---- Scenario history ----------------------------------------------------------------------------------

        function historyVm(overrides) {
            return Object.assign(TestDoubles.makeFakeHistoryVm(), overrides)
        }

        function test_aScenarioWithNoRunsIsNoHistoryWithoutARangeHint() {
            const panel = createTemporaryObject(historyPanelComponent, root, {historyVm: historyVm({runCount: 0, visibleRunCount: 0})})
            compare(find(panel, "historyEmptyLabel").text, "No runs recorded for this scenario.")
            verify(!shown(panel, "historyEmptyHint"))
            verifyGearOpens(gearOf(panel, "scenarioHistory"))
        }

        function test_aRangeHidingEveryRunIsAnEmptyRangeWithAHint() {
            const panel = createTemporaryObject(historyPanelComponent, root, {historyVm: historyVm({runCount: 12, visibleRunCount: 0})})
            compare(find(panel, "historyEmptyLabel").text, "No runs in the selected range.")
            const hint = find(panel, "historyEmptyHint")
            verify(hint !== null && hint.visible)
            verify(hint.text.indexOf("range") >= 0)
            compare(find(panel, "historyGraphCanvas"), null)
            verifyGearOpens(gearOf(panel, "scenarioHistory"))
        }

        function test_visibleRunsHideBothMessagesAndKeepTheGear() {
            const panel = createTemporaryObject(historyPanelComponent, root, {historyVm: historyVm({runCount: 12, visibleRunCount: 8})})
            verify(!shown(panel, "historyEmptyLabel"))
            verify(!shown(panel, "historyEmptyHint"))
            gearOf(panel, "scenarioHistory")
        }

        // ---- Benchmark cards ----------------------------------------------------------------------------------------

        function makeCard(overrides, extra) {
            return createTemporaryObject(cardComponent, root, Object.assign({
                historyModel: TestDoubles.makeFakeHistoryModel(overrides)
            }, extra || {}))
        }

        function test_aBenchmarkWithNoHistoryHasNoRangeHintAndNoNotice() {
            const card = makeCard({hasData: false})
            verify(shown(card, "rankHistoryEmptyLabel"))
            verify(!shown(card, "rankHistoryEmptyHint"))
            verify(!shown(card, "rankHistoryNoImprovementLabel"))
            verifyGearOpens(gearOf(card, "benchmarkRank"))
        }

        function test_anEmptyBenchmarkRangeIsDistinguishedAndPointsToTheGear() {
            const card = makeCard({hasData: false, historyAvailable: true, noObservationsInRange: true,
                                   emptyStateText: "No history in the selected range."})
            compare(find(card, "rankHistoryEmptyLabel").text, "No history in the selected range.")
            const hint = find(card, "rankHistoryEmptyHint")
            verify(hint !== null && hint.visible)
            verify(hint.text.indexOf("range") >= 0)
            verify(!shown(card, "rankHistoryNoImprovementLabel"))
            verifyGearOpens(gearOf(card, "benchmarkRank"))
        }

        function test_pointsOnlyOverACarriedRankExplainsThereAreNoImprovementPoints() {
            const card = makeCard({hasData: true, historyAvailable: true, hasCarriedState: true,
                                   noObservationsInRange: true, noImprovementPoints: true, connectionMode: 2})
            const notice = find(card, "rankHistoryNoImprovementLabel")
            verify(notice !== null && notice.visible, "the empty plot is explained")
            verify(notice.text.indexOf("improvement") >= 0)
            verify(!shown(card, "rankHistoryEmptyLabel"), "this is not the empty-range state")
            verify(!shown(card, "rankHistoryEmptyHint"))
            verifyGearOpens(gearOf(card, "benchmarkRank"))
        }

        function test_theNoImprovementNoticeFollowsTheViewModel() {
            const model = TestDoubles.makeFakeHistoryModel({hasData: true, hasCarriedState: true, historyAvailable: true})
            const card = createTemporaryObject(cardComponent, root, {historyModel: model})
            verify(!shown(card, "rankHistoryNoImprovementLabel"))
            card.historyModel = Object.assign({}, model, {noImprovementPoints: true, connectionMode: 2})
            verify(shown(card, "rankHistoryNoImprovementLabel"))
        }

        function test_aCardWithoutAViewModelStillHasItsGear() {
            const card = createTemporaryObject(cardComponent, root)
            gearOf(card, "benchmarkRank")
        }

        // ---- Benchmark workspace gates (EDGE-06) -----------------------------------------------------------------------

        function makeWorkspace(fixture) {
            const ws = createTemporaryObject(workspaceComponent, root, {
                trackingVm: TestDoubles.makeFakeTrackingVm(fixture), settingsVm: quickVm
            })
            verify(ws !== null)
            return ws
        }

        function test_anIncompleteBenchmarkKeepsItsGateAndItsPlaytimeGear() {
            const fixture = TestDoubles.trackingFixtureIncomplete()
            fixture.playtimeHistory = TestDoubles.makeFakeHistoryModel({
                hasData: false, historyAvailable: true, noObservationsInRange: true,
                emptyStateText: "No history in the selected range."})
            const ws = makeWorkspace(fixture)

            tryVerify(() => find(ws, "incompleteBody") !== null, 2000)
            verify(find(ws, "incompleteBody").visible)
            compare(find(ws, "rankHistoryCard"), null, "completeness gates keep rank hidden")
            compare(find(ws, "graphGear_benchmarkRank"), null, "no rank gear without a rank graph")
            verify(shown(ws, "playtimeHistoryEmptyHint"), "an empty playtime range hints at widening it")
            verifyGearOpens(gearOf(ws, "benchmarkPlaytime"))
        }

        function test_statesWithoutBenchmarkGraphsStayAsTheyWere_data() {
            return [
                {tag: "emptyLibrary", fixture: TestDoubles.trackingFixtureEmptyLibrary(), body: "emptyLibraryBody"},
                {tag: "problemsOnly", fixture: TestDoubles.trackingFixtureProblemsOnly(), body: "problemsOnlyBody"},
                {tag: "noSelection", fixture: TestDoubles.trackingFixtureNoSelection(), body: "noSelectionBody"}
            ]
        }

        function test_statesWithoutBenchmarkGraphsStayAsTheyWere(data) {
            const ws = makeWorkspace(data.fixture)
            tryVerify(() => find(ws, data.body) !== null, 2000)
            compare(find(ws, "rankHistoryCard"), null)
            compare(find(ws, "playtimeHistoryCard"), null)
        }
    }
}
