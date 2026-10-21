import QtQuick
import QtQuick.Controls
import QtTest
import "../../../src/ui/qml/benchmark"
import "ItemLookup.js" as ItemLookup

TestCase {
    id: testCase
    name: "BenchmarkPickerTest"
    when: windowShown
    width: 600
    height: 500
    visible: true

    // Alpha and Beta Voltaic are selectable; the two problem files between and after them are not.
    readonly property var defaultEntries: [
        {id: "id-alpha", name: "Alpha", filename: "alpha.json", classification: "Trackable", selectable: true},
        {id: "", name: "bad.json", filename: "bad.json", classification: "Invalid", selectable: false},
        {id: "id-beta", name: "Beta Voltaic", filename: "beta.json", classification: "Incomplete", selectable: true},
        {id: "", name: "future.json", filename: "future.json", classification: "Unsupported", selectable: false},
        {id: "id-gamma", name: "gamma", filename: "gamma.json", classification: "Trackable", selectable: true}
    ]

    BenchmarkPicker {
        id: picker
        x: 20
        y: 20
        width: 300
        height: 28
    }

    SignalSpy { id: selectedSpy; target: picker; signalName: "benchmarkSelected" }
    SignalSpy { id: manageSpy; target: picker; signalName: "manageBenchmarksRequested" }
    SignalSpy { id: editSpy; target: picker; signalName: "editBenchmarkRequested" }

    function init() {
        picker.popup.close()
        tryVerify(() => !picker.popup.opened)
        picker.entries = defaultEntries
        picker.selectedName = ""
        selectedSpy.clear()
        manageSpy.clear()
        editSpy.clear()
    }

    function cleanup() {
        picker.popup.close()
    }

    function find(name) {
        return ItemLookup.findByObjectName(picker, name) || ItemLookup.findByObjectName(picker.popup.contentItem, name)
    }

    function searchField() { return find("benchmarkSearchField") }

    function openPopup() {
        mouseClick(find("benchmarkSelectorTitle"))
        tryVerify(() => picker.popup.opened, 2000, "popup never opened")
        tryVerify(() => searchField().activeFocus, 2000, "search field never took focus")
        waitForSettledLayout()
    }

    function setSearch(text) {
        searchField().text = text
        waitForSettledLayout()
    }

    // `opened` and a text change both land before the popup's ColumnLayout relayouts. Until it does,
    // the Manage row (declared last, so on top) still sits over the old rows' position: a click on an
    // entry row then hits Manage and closes the popup.
    function waitForSettledLayout() {
        const query = searchField().text.toLowerCase()
        const expected = picker.entries.map(entry => String(entry.name))
            .filter(name => name.toLowerCase().includes(query))
        expectRows(expected)
        const popupItem = picker.popup.contentItem.parent
        const list = find("benchmarkEntryList")
        const manage = find("selectorManageBenchmarks")
        tryVerify(() => {
            const listBottom = list.visible ? list.mapToItem(popupItem, 0, list.height).y : 0
            const manageTop = manage.mapToItem(popupItem, 0, 0).y
            return (!list.visible || list.height === Math.min(list.contentHeight, 280))
                && manageTop >= listBottom
                && manageTop + manage.height <= popupItem.height
        }, 2000, "popup layout never settled")
    }

    // Visible entry rows, top to bottom.
    function rows() {
        const found = ItemLookup.findByObjectNamePrefix(picker.popup.contentItem, "selectorEntry_", [])
            .filter(item => item.visible && item.parent && item.parent.visible)
        found.sort((a, b) => a.parent.y - b.parent.y)
        return found
    }

    function rowNames() { return rows().map(item => item.text) }

    function expectRows(expected) {
        tryVerify(() => JSON.stringify(rowNames()) === JSON.stringify(expected), 2000,
                  "rows were " + JSON.stringify(rowNames()) + ", expected " + JSON.stringify(expected))
    }

    function highlightedNames() { return rows().filter(item => item.highlighted).map(item => item.text) }

    function expectHighlight(name) {
        tryVerify(() => JSON.stringify(highlightedNames()) === JSON.stringify([name]), 2000,
                  "highlighted " + JSON.stringify(highlightedNames()) + ", expected " + name)
    }

    function row(name) { return rows().find(item => item.text === name) || null }

    // -- title ------------------------------------------------------------

    function test_titleShowsTheSelectedName() {
        picker.selectedName = "Alpha"
        const title = find("benchmarkSelectorTitle")
        compare(title.text, "Alpha")
        compare(title.font.bold, true)
        compare(title.color, picker.palette.windowText)
    }

    function test_titleIsAPlaceholderWithNothingSelected() {
        const title = find("benchmarkSelectorTitle")
        compare(title.text, "Select a benchmark")
        compare(title.color, picker.palette.placeholderText)
    }

    // -- opening ----------------------------------------------------------

    function test_clickingTheTitleOpensThePopup() {
        mouseClick(find("benchmarkSelectorTitle"))
        tryVerify(() => picker.popup.opened)
        compare(picker.popup.objectName, "benchmarkSelectorPopup")
    }

    function test_clickingTheChevronOpensThePopup() {
        mouseClick(find("benchmarkSelectorChevron"))
        tryVerify(() => picker.popup.opened)
    }

    function test_chevronPointsUpOnlyWhileOpen() {
        const chevron = find("benchmarkSelectorChevron")
        compare(chevron.rotation, 0)
        openPopup()
        compare(chevron.rotation, 180)
    }

    function test_clickingTheSelectorAgainClosesThePopup() {
        openPopup()
        mouseClick(find("benchmarkSelectorTitle"))
        tryVerify(() => !picker.popup.opened)
    }

    function test_searchFieldTakesFocusAndIsClearedOnEveryOpen() {
        openPopup()
        compare(searchField().text, "")
        setSearch("alp")
        keyClick(Qt.Key_Escape)
        tryVerify(() => !picker.popup.opened)

        openPopup()
        compare(searchField().text, "")
        expectRows(["Alpha", "bad.json", "Beta Voltaic", "future.json", "gamma"])
    }

    function test_searchFieldTextIsTypedIntoTheFocusedField() {
        openPopup()
        for (const character of "gam")
            keyClick(character)
        compare(searchField().text, "gam")
        expectRows(["gamma"])
    }

    // -- filtering --------------------------------------------------------

    function test_filterIsACaseInsensitiveSubstringMatchOnName_data() {
        return [
            {tag: "lowercase query", query: "volt", expected: ["Beta Voltaic"]},
            {tag: "uppercase query", query: "VOLT", expected: ["Beta Voltaic"]},
            {tag: "mixed case against a lowercase name", query: "GaMm", expected: ["gamma"]},
            {tag: "substring in the middle", query: "lph", expected: ["Alpha"]},
            {tag: "several matches", query: "a", expected: ["Alpha", "bad.json", "Beta Voltaic", "gamma"]},
            {tag: "problem files are filtered by name too", query: ".json", expected: ["bad.json", "future.json"]}
        ]
    }

    function test_filterIsACaseInsensitiveSubstringMatchOnName(data) {
        openPopup()
        setSearch(data.query)
        expectRows(data.expected)
        compare(find("selectorNoMatches").visible, false)
    }

    function test_noMatchRowAppearsOnlyWhenNothingMatches() {
        openPopup()
        compare(find("selectorNoMatches").visible, false)
        setSearch("zzz")
        expectRows([])
        const none = find("selectorNoMatches")
        tryVerify(() => none.visible)
        compare(none.text, "No benchmarks match")
        setSearch("")
        tryVerify(() => !none.visible)
    }

    function test_entriesKeepTheGivenOrder() {
        picker.entries = [
            {id: "z", name: "Zed", filename: "z.json", classification: "Trackable", selectable: true},
            {id: "a", name: "Alpha", filename: "a.json", classification: "Trackable", selectable: true},
            {id: "m", name: "Mid Alpha", filename: "m.json", classification: "Trackable", selectable: true}
        ]
        openPopup()
        expectRows(["Zed", "Alpha", "Mid Alpha"])
        setSearch("alpha")
        expectRows(["Alpha", "Mid Alpha"])
    }

    // -- Manage Benchmarks ------------------------------------------------

    function test_manageRowIsVisibleUnderEveryFilter_data() {
        return [
            {tag: "no filter", query: ""},
            {tag: "some matches", query: "alp"},
            {tag: "no matches", query: "zzz"}
        ]
    }

    function test_manageRowIsVisibleUnderEveryFilter(data) {
        openPopup()
        setSearch(data.query)
        const manage = find("selectorManageBenchmarks")
        verify(manage.visible)
        verify(manage.height > 0)
        compare(manage.text, "Manage Benchmarks…")
    }

    function test_manageRowEmitsAndClosesThePopup() {
        openPopup()
        setSearch("zzz")
        mouseClick(find("selectorManageBenchmarks"))
        compare(manageSpy.count, 1)
        compare(selectedSpy.count, 0)
        tryVerify(() => !picker.popup.opened)
    }

    // -- problem rows -----------------------------------------------------

    function test_trackableRowsCarryNoChipOrEditButton() {
        openPopup()
        compare(find("selectorEntryLabel_Alpha").visible, false)
        compare(find("selectorEntryEdit_Alpha").visible, false)
    }

    function test_problemRowsShowAChipAndAnEditButton() {
        openPopup()
        for (const name of ["bad.json", "Beta Voltaic", "future.json"]) {
            verify(find("selectorEntryLabel_" + name).visible, name + " chip")
            verify(find("selectorEntryEdit_" + name).visible, name + " edit button")
        }
        compare(find("selectorEntryLabel_bad.json").children[0].text, "Invalid")
        compare(find("selectorEntryLabel_Beta Voltaic").children[0].text, "Incomplete")
        compare(find("selectorEntryLabel_future.json").children[0].text, "Unsupported")
    }

    function test_invalidAndUnsupportedRowsAreNotSelectable() {
        openPopup()
        for (const name of ["bad.json", "future.json"]) {
            compare(row(name).enabled, false)
            mouseClick(row(name))
        }
        compare(selectedSpy.count, 0)
        verify(picker.popup.opened)
    }

    function test_incompleteRowIsStillSelectable() {
        openPopup()
        mouseClick(row("Beta Voltaic"))
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], "id-beta")
    }

    function test_editButtonEmitsTheFileAndIdWithoutSelecting_data() {
        return [
            {tag: "invalid", name: "bad.json", filename: "bad.json", id: ""},
            {tag: "unsupported", name: "future.json", filename: "future.json", id: ""},
            {tag: "incomplete", name: "Beta Voltaic", filename: "beta.json", id: "id-beta"}
        ]
    }

    function test_editButtonEmitsTheFileAndIdWithoutSelecting(data) {
        openPopup()
        mouseClick(find("selectorEntryEdit_" + data.name))
        compare(editSpy.count, 1)
        compare(editSpy.signalArguments[0][0], data.filename)
        compare(editSpy.signalArguments[0][1], data.id)
        compare(selectedSpy.count, 0)
    }

    // -- selection ---------------------------------------------------------

    function test_selectedRowShowsACheckMarkAndIsHighlighted() {
        picker.selectedName = "gamma"
        openPopup()
        tryVerify(() => find("selectorEntryCheck_gamma") !== null && find("selectorEntryCheck_gamma").visible)
        expectRows(["Alpha", "bad.json", "Beta Voltaic", "future.json", "gamma"])
        compare(find("selectorEntryCheck_Alpha").visible, false)
        expectHighlight("gamma")
    }

    function test_clickingARowSelectsItAndClosesThePopup() {
        openPopup()
        mouseClick(row("gamma"))
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], "id-gamma")
        tryVerify(() => !picker.popup.opened)
    }

    // -- keyboard ---------------------------------------------------------

    function test_arrowKeysSkipUnselectableRowsAndStopAtTheEnds() {
        openPopup()
        expectHighlight("Alpha")
        keyClick(Qt.Key_Up)
        expectHighlight("Alpha")
        keyClick(Qt.Key_Down)
        expectHighlight("Beta Voltaic")
        keyClick(Qt.Key_Down)
        expectHighlight("gamma")
        keyClick(Qt.Key_Down)
        expectHighlight("gamma")
        keyClick(Qt.Key_Up)
        expectHighlight("Beta Voltaic")
    }

    function test_arrowKeysFollowTheFilteredRows() {
        openPopup()
        setSearch("a")
        keyClick(Qt.Key_Down)
        setSearch(".json")
        expectRows(["bad.json", "future.json"])
        compare(highlightedNames().length, 0)
        keyClick(Qt.Key_Down)
        compare(highlightedNames().length, 0)
    }

    function test_enterSelectsTheHighlightedRowAndClosesThePopup() {
        openPopup()
        keyClick(Qt.Key_Down)
        expectHighlight("Beta Voltaic")
        keyClick(Qt.Key_Return)
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], "id-beta")
        tryVerify(() => !picker.popup.opened)
    }

    function test_enterAfterFilteringSelectsTheFirstMatch() {
        openPopup()
        setSearch("gam")
        expectHighlight("gamma")
        keyClick(Qt.Key_Enter)
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], "id-gamma")
    }

    function test_enterWithNothingToSelectDoesNothing() {
        openPopup()
        setSearch("zzz")
        keyClick(Qt.Key_Return)
        compare(selectedSpy.count, 0)
        verify(picker.popup.opened)
        setSearch(".json")
        keyClick(Qt.Key_Return)
        compare(selectedSpy.count, 0)
        verify(picker.popup.opened)
    }

    function test_escapeClosesThePopupWithoutSelecting() {
        openPopup()
        keyClick(Qt.Key_Escape)
        tryVerify(() => !picker.popup.opened)
        compare(selectedSpy.count, 0)
        compare(manageSpy.count, 0)
    }

    function test_manageRowActivatesFromTheKeyboard() {
        openPopup()
        find("selectorManageBenchmarks").forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(manageSpy.count, 1)
        tryVerify(() => !picker.popup.opened)
    }

    function test_reopeningRestartsTheHighlightOnTheSelectedEntry() {
        picker.selectedName = "Alpha"
        openPopup()
        keyClick(Qt.Key_Down)
        expectHighlight("Beta Voltaic")
        keyClick(Qt.Key_Escape)
        tryVerify(() => !picker.popup.opened)
        openPopup()
        expectHighlight("Alpha")
    }
}
