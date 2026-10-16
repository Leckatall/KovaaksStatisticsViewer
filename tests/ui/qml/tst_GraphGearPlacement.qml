import QtQuick
import QtQuick.Controls
import QtTest
import KovaaksStatsViewer
import "../../../src/ui/qml/graph"
import "../../../src/ui/qml/benchmark"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

// The gear sits in each graph's header, outside every loader that needs plottable data, so
// "no data" never takes the way to its settings away.
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
            historyVm: TestDoubles.makeFakeHistoryVm()
            columnVisibility: historySettings.value
            historyAxisSettings: historySettings
            settingsVm: quickVm
        }
    }

    Component {
        id: historyCardComponent
        BenchmarkHistoryCard {
            width: 500
            height: 300
            graphKey: "benchmarkRank"
            namePrefix: "rankHistory"
            settingsVm: quickVm
            historyModel: TestDoubles.makeFakeHistoryModel({hasData: false})
        }
    }

    TestCase {
        name: "GraphGearPlacementTest"
        when: windowShown

        function init() {
            quickVm.patches = []
            quickVm.rejectWith = []
        }

        function gearOf(panel, key) {
            const gear = ItemLookup.findByObjectName(panel, "graphGear_" + key)
            verify(gear !== null, "no graphGear_" + key + " in the panel")
            return gear
        }

        function openAndClickMore(gear, key) {
            mouseClick(gear)
            tryVerify(() => gear.popup.opened, 2000)
            const more = ItemLookup.findByObjectName(gear.popup.contentItem, "graphMoreSettings_" + key)
            verify(more !== null)
            mouseClick(more)
        }

        // ---- Run graph -----------------------------------------------------------------------------

        function test_runGraphHasItsGearWhileNoRunIsSelected() {
            fakeGraphVm.contentState = GraphViewModel.NoRunSelected
            const panel = createTemporaryObject(runPanelComponent, root)
            const gear = gearOf(panel, "run")
            compare(gear.visible, true)
            compare(ItemLookup.findByObjectName(panel, "scenarioGraphCanvas"), null)
        }

        function test_runGraphKeepsItsGraphLinesEntryPointBesideTheGear() {
            const panel = createTemporaryObject(runPanelComponent, root)
            gearOf(panel, "run")
            verify(ItemLookup.findByObjectName(panel, "configureGraphLinesButton") !== null)
        }

        function test_runGraphRelaysMoreGraphSettings() {
            const panel = createTemporaryObject(runPanelComponent, root)
            const spy = createTemporaryObject(spyComponent, root, {target: panel, signalName: "moreGraphSettingsRequested"})
            openAndClickMore(gearOf(panel, "run"), "run")
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "run")
        }

        // ---- Daily playtime ----------------------------------------------------------------------------

        function test_playtimeGraphHasItsGearWithoutPlaytime() {
            fakePlaytimeVm.hasData = false
            const panel = createTemporaryObject(playtimePanelComponent, root)
            compare(gearOf(panel, "dailyPlaytime").visible, true)
            compare(ItemLookup.findByObjectName(panel, "playtimeGraphCanvas"), null)
            compare(ItemLookup.findByObjectName(panel, "playtimeEmptyLabel").visible, true)
        }

        function test_playtimeGraphGearSurvivesDataArriving() {
            fakePlaytimeVm.hasData = false
            const panel = createTemporaryObject(playtimePanelComponent, root)
            const gear = gearOf(panel, "dailyPlaytime")
            fakePlaytimeVm.hasData = true
            tryVerify(() => ItemLookup.findByObjectName(panel, "playtimeGraphCanvas") !== null, 2000)
            compare(gearOf(panel, "dailyPlaytime"), gear)
            fakePlaytimeVm.hasData = false
        }

        function test_playtimeGraphRelaysMoreGraphSettings() {
            const panel = createTemporaryObject(playtimePanelComponent, root)
            const spy = createTemporaryObject(spyComponent, root, {target: panel, signalName: "moreGraphSettingsRequested"})
            openAndClickMore(gearOf(panel, "dailyPlaytime"), "dailyPlaytime")
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "dailyPlaytime")
        }

        // ---- Scenario history --------------------------------------------------------------------------

        function test_historyGraphHasItsGearWhenNoRunIsVisible() {
            const panel = createTemporaryObject(historyPanelComponent, root)
            compare(gearOf(panel, "scenarioHistory").visible, true)
            compare(ItemLookup.findByObjectName(panel, "historyEmptyLabel").visible, true)
            compare(ItemLookup.findByObjectName(panel, "historyGraphCanvas"), null)
        }

        function test_historyGraphRelaysMoreGraphSettings() {
            const panel = createTemporaryObject(historyPanelComponent, root)
            const spy = createTemporaryObject(spyComponent, root, {target: panel, signalName: "moreGraphSettingsRequested"})
            openAndClickMore(gearOf(panel, "scenarioHistory"), "scenarioHistory")
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "scenarioHistory")
        }

        // ---- Benchmark cards ----------------------------------------------------------------------------

        function test_aHistoryCardHasItsGearWithoutData() {
            const card = createTemporaryObject(historyCardComponent, root)
            compare(gearOf(card, "benchmarkRank").visible, true)
            compare(ItemLookup.findByObjectName(card, "rankHistoryEmptyLabel").visible, true)
        }

        function test_aHistoryCardNamesItsGraphFromItsKey() {
            const card = createTemporaryObject(historyCardComponent, root, {graphKey: "benchmarkPlaytime"})
            const gear = gearOf(card, "benchmarkPlaytime")
            compare(gear.Accessible.name, "Benchmark playtime settings")
        }

        function test_aHistoryCardRelaysMoreGraphSettings() {
            const card = createTemporaryObject(historyCardComponent, root)
            const spy = createTemporaryObject(spyComponent, root, {target: card, signalName: "moreGraphSettingsRequested"})
            openAndClickMore(gearOf(card, "benchmarkRank"), "benchmarkRank")
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "benchmarkRank")
        }

        function makeWorkspace(fixture) {
            const ws = createTemporaryObject(workspaceComponent, root, {
                trackingVm: TestDoubles.makeFakeTrackingVm(fixture), settingsVm: quickVm
            })
            verify(ws !== null)
            return ws
        }

        function test_trackableBenchmarkHasAGearOnEachHistoryCard() {
            const fixture = TestDoubles.trackingFixtureTrackable()
            fixture.rankHistory = TestDoubles.makeFakeHistoryModel({hasData: false})
            fixture.playtimeHistory = TestDoubles.makeFakeHistoryModel({hasData: false})
            const ws = makeWorkspace(fixture)

            tryVerify(() => ItemLookup.findByObjectName(ws, "graphGear_benchmarkRank") !== null, 2000)
            tryVerify(() => ItemLookup.findByObjectName(ws, "graphGear_benchmarkPlaytime") !== null, 2000)
        }

        function test_incompleteBenchmarkHasAGearOnItsPlaytimeCard() {
            const ws = makeWorkspace(TestDoubles.trackingFixtureIncomplete())

            tryVerify(() => ItemLookup.findByObjectName(ws, "graphGear_benchmarkPlaytime") !== null, 2000)
            compare(ItemLookup.findByObjectName(ws, "graphGear_benchmarkRank"), null)
        }

        function test_theWorkspaceRelaysMoreGraphSettingsFromEitherBody() {
            const trackable = makeWorkspace(TestDoubles.trackingFixtureTrackable())
            const spy = createTemporaryObject(spyComponent, root, {target: trackable, signalName: "moreGraphSettingsRequested"})
            tryVerify(() => ItemLookup.findByObjectName(trackable, "graphGear_benchmarkRank") !== null, 2000)
            openAndClickMore(ItemLookup.findByObjectName(trackable, "graphGear_benchmarkRank"), "benchmarkRank")
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0], "benchmarkRank")

            const incomplete = makeWorkspace(TestDoubles.trackingFixtureIncomplete())
            const incompleteSpy = createTemporaryObject(spyComponent, root, {target: incomplete, signalName: "moreGraphSettingsRequested"})
            tryVerify(() => ItemLookup.findByObjectName(incomplete, "graphGear_benchmarkPlaytime") !== null, 2000)
            openAndClickMore(ItemLookup.findByObjectName(incomplete, "graphGear_benchmarkPlaytime"), "benchmarkPlaytime")
            compare(incompleteSpy.count, 1)
            compare(incompleteSpy.signalArguments[0][0], "benchmarkPlaytime")
        }
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }

    Component {
        id: workspaceComponent
        BenchmarkWorkspace {
            width: 1200
            height: 800
        }
    }
}
