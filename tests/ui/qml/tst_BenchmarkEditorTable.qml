import QtQuick
import QtTest
import QtQuick.Controls
import KsvTestSupport
import "../../../src/ui/qml"
import "ItemLookup.js" as ItemLookup
import "TestDoubles.js" as TestDoubles

TestCase {
    id: testCase
    name: "BenchmarkEditorTableTest"
    when: windowShown
    width: 900
    height: 520
    visible: true

    Component {
        id: fixtureComponent
        BenchmarkTableModelFixture {}
    }

    Component {
        id: tableComponent
        BenchmarkEditorTable {
            width: 900
            height: 520
        }
    }

    SignalSpy {
        id: resetSpy
        signalName: "modelReset"
    }

    // `fixtureRef`, when given, receives the fixture so a command double can publish a new
    // projection through it and genuinely reset the model.
    function makeTable(desc, overrides, fixtureRef) {
        const fixture = createTemporaryObject(fixtureComponent, testCase)
        if (fixtureRef) fixtureRef.fixture = fixture
        fixture.setProjection(TestDoubles.benchmarkTableProjection(desc))
        const manager = TestDoubles.makeFakeTableManager(fixture, overrides)
        const table = createTemporaryObject(tableComponent, testCase, {manager: manager})
        verify(table !== null, "BenchmarkEditorTable failed to instantiate")
        verify(waitForRendering(table), "table never rendered")
        return table
    }

    function cells(view) {
        return view ? ItemLookup.findByObjectNamePrefix(view.contentItem, "cell_", []) : []
    }

    function cellFor(table, entryId, column) {
        const views = [table.identityView, table.thresholdView]
        for (const view of views) {
            const found = view ? ItemLookup.findByObjectName(view.contentItem, "cell_" + entryId + "_" + column) : null
            if (found && found.visible) return found
        }
        return null
    }

    function liveCellCount(table) {
        return cells(table.identityView).length + cells(table.thresholdView).length
    }

    function scrollPathDelegateCount(table, middleRow, farRow) {
        for (const row of [middleRow, farRow, 0]) {
            table.thresholdView.positionViewAtRow(row, TableView.AlignTop)
            waitForRendering(table)
        }
        return liveCellCount(table)
    }

    // The sticky label of a group span is shown by exactly one live cell of that span.
    function shownGroupLabels(table, groupId) {
        return cells(table.identityView).filter(c => c.visible && c.groupId === groupId && c.groupLabelShown)
    }

    function test_compactRowsHeadersAndScrolledSpans() {
        const desc = TestDoubles.benchmarkTableLargeDesc(200, 151, 170)
        const table = makeTable(desc, {groups: [
            {id: "g1", kind: "category", parentId: "", name: "Category g1", scenarioCount: 151, issues: []},
            {id: "g2", kind: "category", parentId: "", name: "Category g2", scenarioCount: 49, issues: []},
            {id: "gEmpty", kind: "category", parentId: "", name: "Empty", scenarioCount: 0,
             issues: ["A category has neither scenarios nor subcategories."]}
        ]})
        verify(table.identityView, "the table must expose its identifying-column view")
        verify(table.thresholdView, "the table must expose its threshold view")
        compare(table.thresholdView.rows, 200, "one body row per scenario")

        // One shared rank header across all rows, carrying the tier names.
        for (const tier of ["t1", "t2", "t3"]) {
            const header = ItemLookup.findByObjectName(table, "rankHeader_" + tier)
            verify(header !== null, "missing shared header for " + tier)
        }
        compare(ItemLookup.findByObjectName(table, "rankHeader_t2").headerName, "Silver")

        // The empty group has no row but stays reachable in the group list.
        verify(ItemLookup.findByObjectName(table, "groupListItem_gEmpty") !== null,
               "empty groups must remain listed")

        // Scroll into the middle of the long g1 span: its name is still readable exactly once.
        table.thresholdView.positionViewAtRow(100, TableView.AlignTop)
        tryVerify(() => table.identityView.topRow === table.thresholdView.topRow && table.thresholdView.topRow >= 99)
        waitForRendering(table)
        const labels = shownGroupLabels(table, "g1")
        compare(labels.length, 1, "a clipped span keeps one readable label")
        compare(labels[0].groupLabelText, "Category g1")
        verify(labels[0].mapToItem(table, 0, 0).y >= 0, "the label sits inside the viewport")

        // Scroll to the invalid row; the recycled delegate shows the retained text.
        table.thresholdView.positionViewAtRow(170, TableView.AlignVCenter)
        tryVerify(() => cellFor(table, "e170", "t2") !== null)
        const invalid = cellFor(table, "e170", "t2")
        compare(invalid.cellText, "12oops")
        compare(invalid.stateLabel, qsTr("Invalid input"))

        // Horizontal inspection keeps the identifying columns in place.
        table.thresholdView.contentX = table.thresholdView.contentWidth
        waitForRendering(table)
        const scenarioCell = cellFor(table, "e170", "scenario")
        verify(scenarioCell !== null, "the scenario label stays live while thresholds scroll")
        verify(scenarioCell.mapToItem(table, 0, 0).x >= 0 && scenarioCell.mapToItem(table, 0, 0).x < 400)

        // Bounded delegates: after the same scroll path through the same viewport, a table with 10x
        // the rows holds the same number of delegate objects (visible plus TableView's reuse pool).
        // The view starts past the identifying columns it does not draw (a negative left margin).
        table.thresholdView.contentX = -table.thresholdView.leftMargin
        const small = scrollPathDelegateCount(table, 100, 170)
        table.destroy()
        const big = makeTable(TestDoubles.benchmarkTableLargeDesc(2000, 1500, 1700))
        const large = scrollPathDelegateCount(big, 1000, 1700)
        verify(small > 0)
        // The reuse pool may differ by a row or two of delegates (6 columns each); it never scales
        // with the 1,800 extra rows.
        verify(Math.abs(large - small) <= 12,
               "delegate count must follow the viewport, not the row count: " + small + " vs " + large)
    }

    function test_selectionTintAndContextualPlusControls() {
        const group = {id: "g1", name: "Clicking", color: "#009600"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: group, name: "Alpha", cells: {t1: {displayText: "1"}}},
            {entryId: "b", category: group, name: "Beta", cells: {t1: {displayText: "2"}}}
        ]}, {groups: [
            {id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 2, issues: []},
            {id: "empty", kind: "category", parentId: "", name: "Empty", scenarioCount: 0, issues: []},
            {id: "uncategorized", kind: "uncategorized", parentId: "", name: "Uncategorized", scenarioCount: 0, issues: []}
        ]})

        verify(table.focusCell({entryId: "a", tierId: "t1"}))
        tryVerify(() => cellFor(table, "a", "scenario").rowHighlighted)
        verify(cellFor(table, "a", "t1").current)
        verify(!cellFor(table, "b", "scenario").rowHighlighted)
        verify(ItemLookup.findByObjectName(table, "addCategoryPlus") !== null)
        verify(ItemLookup.findByObjectName(table, "addScenarioPlus") !== null)
        verify(ItemLookup.findByObjectName(table, "addRankPlus") !== null)
        verify(ItemLookup.findByObjectName(table, "addSubcategoryPlus_g1") !== null)
        verify(ItemLookup.findByObjectName(table, "groupListItem_empty") !== null)
        verify(ItemLookup.findByObjectName(table, "groupListItem_g1") === null)
        verify(ItemLookup.findByObjectName(table, "groupListItem_uncategorized") === null)
    }

    // A category whose scenarios sit straight under it (and Uncategorized) spans the Subcategory
    // column; one with subcategories keeps its own cell. The span stays selectable, editable and
    // reachable from the keyboard, and its add-subcategory control remains.
    function test_categoryWithoutSubcategoriesSpansTheSubcategoryColumn() {
        const clicking = {id: "g1", name: "Clicking", color: "#009600"}
        const tracking = {id: "g2", name: "Tracking", color: "#960096"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: clicking, subcategory: {id: "s1", name: "Static"}, name: "Alpha", cells: {}},
            {entryId: "b", category: tracking, name: "Beta", cells: {}},
            {entryId: "c", category: tracking, name: "Gamma", cells: {}},
            {entryId: "d", name: "Delta", cells: {}}
        ]}, {groups: [
            {id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 1, issues: []},
            {id: "s1", kind: "subcategory", parentId: "g1", name: "Static", scenarioCount: 1, issues: []},
            {id: "g2", kind: "category", parentId: "", name: "Tracking", scenarioCount: 2, issues: []}
        ]})
        const spanWidth = table.identityWidths[0] + table.identityWidths[1]

        const withSubcategory = cellFor(table, "a", "category")
        verify(!withSubcategory.spansBoth)
        compare(withSubcategory.face.width, table.identityWidths[0])
        verify(!cellFor(table, "a", "subcategory").coveredBySpan)
        compare(cellFor(table, "a", "subcategory").opacity, 1)

        for (const entryId of ["b", "c", "d"]) {
            const category = cellFor(table, entryId, "category")
            verify(category.spansBoth, entryId + " has no subcategory, so its category spans")
            compare(category.face.width, spanWidth)
            const covered = cellFor(table, entryId, "subcategory")
            verify(covered.coveredBySpan)
            compare(covered.opacity, 0)
            verify(!covered.enabled)
        }
        verify(ItemLookup.findByObjectName(table, "addSubcategoryPlus_g2").visible,
               "a spanning category can still gain a subcategory")

        // A click on the Subcategory column's part of the span selects the category.
        const tracking0 = cellFor(table, "b", "category")
        mouseClick(tracking0.face, table.identityWidths[0] + table.identityWidths[1] / 2, tracking0.height / 2)
        tryCompare(table.selectionModel.currentIndex, "column", 0)
        compare(table.selectionModel.currentIndex.row, 1)
        compare(table.focusedGroupId, "g2")

        // The keyboard steps over the covered cell, and never comes to rest on one.
        keyClick(Qt.Key_Right)
        compare(table.selectionModel.currentIndex.column, 2)
        keyClick(Qt.Key_Left)
        compare(table.selectionModel.currentIndex.column, 0)
        verify(table.focusCell({entryId: "a", columnKind: 1}))
        keyClick(Qt.Key_Down)
        compare(table.selectionModel.currentIndex.row, 1)
        compare(table.selectionModel.currentIndex.column, 0)

        // The name is edited across the whole span.
        verify(table.beginGroupRename("g2"))
        tryVerify(() => table.activeEditor !== null)
        compare(table.activeEditor.width, spanWidth)
        keyClick(Qt.Key_Escape)
    }

    // Return in the add-subcategory field is the field's own: it used to propagate to the table,
    // whose edit trigger opened a rename editor on the current category cell, behind the prompt
    // the Manager opens for a category that already holds scenarios.
    function test_returnInSubcategoryFieldOpensNoRenameEditor() {
        const tracking = {id: "g2", name: "Tracking", color: "#960096"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "b", category: tracking, name: "Beta", cells: {}},
            {entryId: "c", category: tracking, name: "Gamma", cells: {}}
        ]}, {groups: [
            {id: "g2", kind: "category", parentId: "", name: "Tracking", scenarioCount: 2, issues: []}
        ]})
        const requests = []
        table.addSubcategoryRequested.connect((categoryId, name) => {
            requests.push([categoryId, name])
            table.creationAccepted = true
        })

        const category = cellFor(table, "b", "category")
        mouseClick(category.face, 20, category.height / 2)
        tryCompare(table.selectionModel.currentIndex, "column", 0)
        mouseClick(ItemLookup.findByObjectName(table, "addSubcategoryPlus_g2"))
        const field = category.subcategoryField
        tryVerify(() => field.visible && field.activeFocus)
        keyClick(Qt.Key_S)
        keyClick(Qt.Key_Return)

        compare(requests, [["g2", "s"]])
        waitForRendering(table)
        compare(table.activeEditor, null, "Return in the name field must not reach the table's edit trigger")
        verify(!field.visible)
        verify(!field.focus, "the hidden field no longer holds the table's focus")
        verify(table.identityView.activeFocus)
        compare(table.selectionModel.currentIndex.row, 0)
        compare(table.selectionModel.currentIndex.column, 0)

        // Escape closes the field the same way, without creating anything.
        mouseClick(ItemLookup.findByObjectName(table, "addSubcategoryPlus_g2"))
        tryVerify(() => field.visible && field.activeFocus)
        keyClick(Qt.Key_X)
        keyClick(Qt.Key_Escape)
        verify(!field.visible)
        compare(field.text, "")
        verify(table.identityView.activeFocus)
        compare(requests.length, 1)

        // Return on the category cell itself still renames it.
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        compare(table.activeEditor.text, "Tracking")
        keyClick(Qt.Key_Escape)
        tryVerify(() => table.activeEditor === null)
    }

    function test_dragSelectionMovesRowsOnlyToValidGroupOrUncategorized() {
        const group = {id: "g1", name: "Clicking"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: group, name: "Alpha", cells: {}},
            {entryId: "b", category: group, name: "Beta", cells: {}},
            {entryId: "c", name: "Gamma", cells: {}}
        ]}, {groups: [
            {id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 2},
            {id: "g2", kind: "category", parentId: "", name: "Tracking", scenarioCount: 0},
            {id: "g3", kind: "category", parentId: "", name: "Divided", scenarioCount: 1},
            {id: "sub", kind: "subcategory", parentId: "g3", name: "Precise", scenarioCount: 1}
        ]})
        const moves = []
        table.moveSelectionRequested.connect((ids, target) => moves.push([ids, target]))
        verify(table.focusCell({entryId: "a", columnKind: 2}))
        table.selectionModel.select(table.tableModel.index(table.tableModel.rowForEntry("b"), 2),
                                    ItemSelectionModel.Select)
        table.beginRowDrag("a")
        verify(ItemLookup.findByObjectName(table, "uncategorizedDropZone").visible)
        table.dropRows("g3")
        compare(moves.length, 0, "a divided category must reject the drop")
        const categoryCell = cellFor(table, "a", "category")
        const categoryPoint = categoryCell.mapToItem(null, categoryCell.width / 2, categoryCell.height / 2)
        compare(table.targetAtScene(categoryPoint), "g1", "the spanning category cell is a drop target")
        table.dropRows("g2")
        compare(moves, [[["a", "b"], "g2"]])
        table.beginRowDrag("c")
        const zone = ItemLookup.findByObjectName(table, "uncategorizedDropZone")
        waitForRendering(table)
        const zonePoint = zone.mapToItem(null, zone.width / 2, zone.height / 2)
        table.dropAtScene(zonePoint)
        compare(moves[1], [["c"], ""])
    }

    function test_pointerDragOntoEmptyGroupMovesSelectedRows() {
        const group = {id: "g1", name: "Clicking"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: group, name: "Alpha", cells: {}},
            {entryId: "b", category: group, name: "Beta", cells: {}}
        ]}, {groups: [
            {id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 2},
            {id: "g2", kind: "category", parentId: "", name: "Tracking", scenarioCount: 0}
        ]})
        const moves = []
        const dragStates = []
        const hoverTargets = []
        table.moveSelectionRequested.connect((ids, target) => moves.push([ids, target]))
        table.draggingRowsChanged.connect(() => dragStates.push(table.draggingRows))
        table.hoverDropGroupIdChanged.connect(() => hoverTargets.push(table.hoverDropGroupId))
        verify(table.focusCell({entryId: "a", columnKind: 2}))
        table.selectionModel.select(table.tableModel.index(table.tableModel.rowForEntry("b"), 2),
                                    ItemSelectionModel.Select)
        const source = cellFor(table, "a", "scenario")
        const target = ItemLookup.findByObjectName(table, "groupListItem_g2")
        verify(source !== null && target !== null)
        const start = source.mapToItem(table, source.width / 2, source.height / 2)
        const end = target.mapToItem(table, target.width / 2, target.height / 2)
        mouseDrag(source, source.width / 2, source.height / 2,
                  end.x - start.x, end.y - start.y, Qt.LeftButton)
        verify(dragStates.indexOf(true) !== -1, "the scenario drag handler must activate")
        verify(hoverTargets.indexOf("g2") !== -1, "the empty group's drop area must receive the drag")
        tryCompare(moves, "length", 1)
        compare(moves[0], [["a", "b"], "g2"])
    }

    function scenePointIn(cell, fraction) {
        return cell.mapToItem(null, cell.width / 2, cell.height * fraction)
    }

    function test_rowInsertionTargetsTheGapAboveOrBelowTheHoveredRow() {
        const group = {id: "g1", name: "Clicking"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: group, name: "Alpha", cells: {}},
            {entryId: "b", category: group, name: "Beta", cells: {}},
            {entryId: "c", name: "Gamma", cells: {}}
        ]}, {groups: [{id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 2}]})
        const insertion = (entryId, column, fraction) => {
            const found = table.rowInsertionAtScene(scenePointIn(cellFor(table, entryId, column), fraction))
            return found ? [found.groupId, found.beforeEntryId] : null
        }

        compare(insertion("b", "scenario", 0.25), ["g1", "b"])
        compare(insertion("a", "scenario", 0.75), ["g1", "b"])
        compare(insertion("b", "scenario", 0.75), ["g1", ""], "below a group's last row appends to it")
        compare(insertion("c", "t1", 0.25), ["", "c"], "threshold cells are row targets too")
        compare(insertion("a", "category", 0.5), null, "group cells keep their append-to-group meaning")
    }

    function test_pointerDragBetweenRowsPlacesTheRowAndShowsWhereItLands() {
        const group = {id: "g1", name: "Clicking"}
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", category: group, name: "Alpha", cells: {}},
            {entryId: "b", category: group, name: "Beta", cells: {}},
            {entryId: "c", name: "Gamma", cells: {}}
        ]}, {groups: [{id: "g1", kind: "category", parentId: "", name: "Clicking", scenarioCount: 2}]})
        const moves = []
        const indicators = []
        table.moveSelectionRequested.connect((ids, target, before) => moves.push([ids, target, before]))
        table.dropInsertionChanged.connect(() => {
            if (table.dropInsertion) indicators.push([table.dropInsertion.row, table.dropInsertion.above])
        })
        const source = cellFor(table, "c", "scenario")
        const target = cellFor(table, "a", "scenario")
        const start = source.mapToItem(table, source.width / 2, source.height / 2)
        const end = target.mapToItem(table, target.width / 2, target.height * 0.25)
        mouseDrag(source, source.width / 2, source.height / 2, end.x - start.x, end.y - start.y, Qt.LeftButton)

        tryCompare(moves, "length", 1)
        compare(moves[0], [["c"], "g1", "a"])
        verify(indicators.some(i => i[0] === 0 && i[1]), "the line must mark the gap above row a")
        compare(table.dropInsertion, null, "the line is gone once the drop lands")
    }

    function test_controlClickSelectsSeparateCellsAndHighlightsBothRows() {
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", name: "Alpha", cells: {t1: {displayText: "1"}}},
            {entryId: "b", name: "Beta", cells: {t1: {displayText: "2"}}}
        ]})
        const first = cellFor(table, "a", "t1")
        const second = cellFor(table, "b", "scenario")
        mouseClick(first)
        mouseClick(second, second.width / 2, second.height / 2, Qt.LeftButton, Qt.ControlModifier)
        tryCompare(table.manager, "selectedEntryIds", ["a", "b"])
        verify(first.rowHighlighted && second.rowHighlighted)
        verify(cellFor(table, "a", "scenario").rowHighlighted)
    }

    function test_modelResetRestoresTheSelectedCellsByEntryAndRank() {
        const ref = {}
        const rows = order => order.map(id => ({entryId: id, name: id, cells: {}}))
        const tiers = order => order.map(id => ({id: id, name: id}))
        const table = makeTable({tiers: tiers(["t1", "t2"]), rows: rows(["a", "b"])}, {}, ref)
        table.selectionModel.select(table.tableModel.index(0, 3), ItemSelectionModel.Select)
        table.selectionModel.select(table.tableModel.index(1, 4), ItemSelectionModel.Select)
        tryCompare(table.manager, "selectedEntryIds", ["a", "b"])

        ref.fixture.setProjection(TestDoubles.benchmarkTableProjection({
            tiers: tiers(["t2", "t1"]), rows: rows(["b", "a"])
        }))
        tryVerify(() => table.selectionModel.selectedIndexes.length === 2)
        const anchors = table.selectionModel.selectedIndexes.map(index => table.tableModel.anchorAt(index.row, index.column))
        verify(anchors.some(anchor => anchor.entryId === "a" && anchor.tierId === "t1"))
        verify(anchors.some(anchor => anchor.entryId === "b" && anchor.tierId === "t2"))
    }

    function test_shiftClickSelectsCellRectangleAcrossRowsAndRanks() {
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}, {id: "t2", name: "Silver"}], rows: [
            {entryId: "a", name: "Alpha", cells: {}},
            {entryId: "b", name: "Beta", cells: {}},
            {entryId: "c", name: "Gamma", cells: {}}
        ]})
        const first = cellFor(table, "a", "t1")
        const last = cellFor(table, "c", "t2")
        mouseClick(first)
        mouseClick(last, last.width / 2, last.height / 2, Qt.LeftButton, Qt.ShiftModifier)
        tryCompare(table.manager, "selectedEntryIds", ["a", "b", "c"])
        compare(table.selectionModel.selectedIndexes.length, 6)
    }

    function test_keyboardEditAndIssueNavigation() {
        const rows = [
            {entryId: "a", cells: {t1: {displayText: "1,234.5678901234567", editText: "1234.5678901234567"},
                                   t2: {displayText: "2,000"}}},
            {entryId: "b", cells: {t1: {displayText: "", hasValue: false,
                                        issues: ["A scenario is missing a threshold for some tier."]},
                                   t2: {displayText: "3"}}}
        ]
        for (let i = 0; i < 60; ++i) rows.push({entryId: "f" + i, cells: {t1: {displayText: "1"}, t2: {displayText: "2"}}})
        rows.push({entryId: "c", cells: {t1: {displayText: "1"},
                                         t2: {displayText: "oops", inputState: "invalid",
                                              issues: ["Not a number. Saving stores this threshold as missing."]}}})
        const table = makeTable({
            tiers: [{id: "t1", name: "Bronze"}, {id: "t2", name: "Silver", issues: ["Two tiers share the same name."]}],
            rows: rows
        }, {groups: [{id: "gEmpty", kind: "category", parentId: "", name: "Empty", scenarioCount: 0,
                      issues: ["A category has neither scenarios nor subcategories."]}]})

        verify(table.focusCell({entryId: "a", tierId: "t1"}), "focusCell must accept a stable cell anchor")
        tryVerify(() => table.manager.currentCell.entryId === "a")
        compare(table.manager.currentCell.tierId, "t1")
        const selected = cellFor(table, "a", "t1")
        compare(selected.stateLabel, qsTr("Selected"))
        compare(cellFor(table, "b", "t1").stateLabel, qsTr("Missing threshold"))

        // Plain keyboard movement forwards no edit.
        keyClick(Qt.Key_Right)
        tryVerify(() => table.manager.currentCell.tierId === "t2")
        keyClick(Qt.Key_Down)
        tryVerify(() => table.manager.currentCell.entryId === "b")
        keyClick(Qt.Key_Up)
        keyClick(Qt.Key_Left)
        tryVerify(() => table.manager.currentCell.tierId === "t1")
        compare(table.manager.commandLog.length, 0, "focus movement is not an edit")

        // The active editor shows the precise ungrouped value and commits it once.
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        compare(table.activeEditor.text, "1234.5678901234567")
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor === null)
        compare(table.manager.editThresholdTextCalls, [["a", "t1", "1234.5678901234567"]])

        // Issue navigation scrolls to cells, headers and groups by stable ID.
        verify(table.focusCell({entryId: "c", tierId: "t2"}))
        tryVerify(() => cellFor(table, "c", "t2") !== null)
        compare(table.manager.currentCell.entryId, "c")
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        compare(table.activeEditor.text, "oops")
        table.activeEditor.selectAll()
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor === null)
        compare(table.manager.editThresholdTextCalls[1], ["c", "t2", "42"])

        verify(table.focusCell({tierId: "t2"}))
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t2").activeFocus)
        verify(table.focusCell({groupId: "gEmpty"}))
        tryVerify(() => ItemLookup.findByObjectName(table, "groupListItem_gEmpty").activeFocus)
    }

    function pasteRows() {
        const g1 = {id: "g1", name: "Clicking"}
        return {
            tiers: [{id: "t1", name: "Bronze"}, {id: "t2", name: "Silver"}],
            rows: [
                {entryId: "a", category: g1, cells: {t1: {displayText: "1"}, t2: {displayText: "2"}}},
                {entryId: "b", category: g1, cells: {t1: {displayText: "3"}}}
            ]
        }
    }

    function typeText(text) {
        for (const ch of text) keyClick(ch)
    }

    function test_pasteCommitsActiveCellAndUsesDestination() {
        const table = makeTable(pasteRows(), {
            clipboardText: function () { return "5	6
7	8" },
            undo: function () {
                this.commandLog.push("undo")
                this.undoCalls.push([])
                this.currentCell = {entryId: "a", tierId: "t1", columnKind: 3}
                return {ok: true}
            }
        })
        verify(typeof table.pasteFromClipboard === "function", "the table must offer a paste command")
        verify(typeof table.undo === "function", "the table must offer an undo command")

        // Typed text still in the editor is committed before the paste captures its destination.
        verify(table.focusCell({entryId: "a", tierId: "t1"}))
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        table.activeEditor.selectAll()
        typeText("12oops")
        table.pasteFromClipboard()
        compare(table.manager.commandLog, ["editThresholdText", "pasteText"])
        compare(table.manager.editThresholdTextCalls[0], ["a", "t1", "12oops"])
        const destination = table.manager.pasteTextCalls[0][0]
        compare(destination.kind, "threshold")
        compare(destination.entryId, "a")
        compare(destination.tierId, "t1")
        compare(table.manager.pasteTextCalls[0][1], "5	6
7	8")
        verify(table.activeEditor === null)

        // The keyboard route pastes a body block starting at the Scenario column.
        verify(table.focusCell({entryId: "b"}))
        keySequence(StandardKey.Paste)
        tryVerify(() => table.manager.pasteTextCalls.length === 2)
        compare(table.manager.pasteTextCalls[1][0].kind, "scenario")
        compare(table.manager.pasteTextCalls[1][0].entryId, "b")

        // A later manual edit, then two Undo steps through the shared history.
        verify(table.focusCell({entryId: "b", tierId: "t2"}))
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        typeText("9")
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor === null)
        keySequence(StandardKey.Undo)
        keySequence(StandardKey.Undo)
        tryVerify(() => table.manager.undoCalls.length === 2)
        compare(table.manager.commandLog.slice(-3), ["editThresholdText", "undo", "undo"])

        // Focus follows the anchor the session restored.
        tryVerify(() => table.selectionModel.currentIndex.row === 0 && table.selectionModel.currentIndex.column === 3)
    }

    function test_rejectedPlacementExplainsWithoutShift() {
        const table = makeTable(pasteRows(), {
            clipboardText: function () { return "\"quoted\ttab\"\t\r\n\t" },
            pasteResult: {ok: false, error: "Paste into scenario, threshold or rank-name cells."}
        })
        verify(typeof table.pasteFromClipboard === "function", "the table must offer a paste command")

        // An edit left open in another cell is committed as its own step before the destination moves.
        verify(table.focusCell({entryId: "b", tierId: "t2"}))
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        typeText("oops")
        verify(table.focusCell({entryId: "a", columnKind: 0}))
        tryVerify(() => table.manager.editThresholdTextCalls.length === 1)
        compare(table.manager.editThresholdTextCalls[0], ["b", "t2", "oops"])

        table.pasteFromClipboard()
        compare(table.manager.pasteTextCalls.length, 1)
        const destination = table.manager.pasteTextCalls[0][0]
        compare(destination.kind, "category")
        compare(destination.groupId, "g1")
        compare(table.manager.pasteTextCalls[0][1], "\"quoted\ttab\"\t\r\n\t", "the raw text goes to the session unaltered")
        compare(table.statusText, "Paste into scenario, threshold or rank-name cells.")
        compare(table.manager.commandLog, ["editThresholdText", "pasteText"], "a rejected paste adds no other command")
        compare(table.manager.currentCell.entryId, "a", "the destination does not shift")

        // Rank names paste horizontally into the header row.
        verify(table.focusCell({tierId: "t2"}))
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t2").activeFocus)
        table.pasteFromClipboard()
        compare(table.manager.pasteTextCalls[1][0].kind, "rankHeader")
        compare(table.manager.pasteTextCalls[1][0].tierId, "t2")
    }

    // ---- Session selection through resets and Undo ------------------------------------------------

    function selectedIdsOf(table) {
        const ids = []
        for (const index of table.selectionModel.selectedIndexes) {
            const id = table.tableModel.anchorAt(index.row, index.column).entryId
            if (id && ids.indexOf(id) === -1) ids.push(id)
        }
        return ids.sort()
    }

    function currentAnchorOf(table) {
        const current = table.selectionModel.currentIndex
        return current.valid ? table.tableModel.anchorAt(current.row, current.column) : ({})
    }

    function isCurrent(table, entryId, column) {
        const current = table.selectionModel.currentIndex
        return current.valid && current.row === table.tableModel.rowForEntry(entryId) && current.column === column
    }

    function selectRow(table, entryId) {
        table.selectionModel.select(table.tableModel.index(table.tableModel.rowForEntry(entryId), 2),
                                    ItemSelectionModel.Select | ItemSelectionModel.Rows)
    }

    function resetDesc(rowOrder, tierOrder, groupOf) {
        const groups = {g1: {id: "g1", name: "Clicking"}, g2: {id: "g2", name: "Tracking"}}
        return {
            tiers: tierOrder.map(id => ({id: id, name: "Rank " + id})),
            rows: rowOrder.map(id => ({entryId: id, category: groups[groupOf[id]] || null,
                                       cells: {t1: {displayText: "1"}, t2: {displayText: "2"}, t3: {displayText: "3"}}}))
        }
    }

    function expectSessionSurvives(table, label) {
        tryVerify(() => { const a = currentAnchorOf(table); return a.entryId === "a" && a.tierId === "t2" },
                  3000, label + ": the current cell follows its IDs")
        tryVerify(() => JSON.stringify(selectedIdsOf(table)) === JSON.stringify(["a", "b"]),
                  3000, label + ": every selected row survives")
        compare(table.manager.selectedEntryIds.slice().sort(), ["a", "b"], label)
        compare(table.manager.currentCell.entryId, "a", label)
        compare(table.manager.currentCell.tierId, "t2", label)
    }

    function test_resetKeepsCurrentCellSelectionAndPasteDestination() {
        const ref = {}
        const grouped = {a: "g1", b: "g1", c: "g2"}
        const table = makeTable(resetDesc(["a", "b", "c"], ["t1", "t2"], grouped), {
            clipboardText: function () { return "7\t8\n9\t10" },
            pasteText: function (destination, text) {
                this.commandLog.push("pasteText")
                this.pasteTextCalls.push([destination, text])
                if (this.pasteTextCalls.length === 1)
                    ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(
                        resetDesc(["b", "c", "a", "n1"], ["t2", "t1", "t3"], {a: "g2", b: "g1", c: "g2"})))
                return {ok: true}
            }
        }, ref)
        resetSpy.target = table.tableModel
        resetSpy.clear()

        verify(table.focusCell({entryId: "a", tierId: "t2"}))
        selectRow(table, "b")
        tryVerify(() => JSON.stringify(table.manager.selectedEntryIds.slice().sort()) === JSON.stringify(["a", "b"]))

        const publish = (rows, tiers, groupOf) =>
            ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(resetDesc(rows, tiers, groupOf)))

        publish(["c", "a", "b"], ["t1", "t2"], {a: "g1", b: "g1", c: "g2"})
        compare(resetSpy.count, 1, "reordering rows resets the model")
        expectSessionSurvives(table, "row reorder")

        publish(["c", "a", "b"], ["t2", "t1"], {a: "g1", b: "g1", c: "g2"})
        compare(resetSpy.count, 2, "reordering ranks resets the model")
        expectSessionSurvives(table, "rank reorder")

        publish(["b", "c", "a"], ["t2", "t1"], {a: "g2", b: "g1", c: "g2"})
        compare(resetSpy.count, 3, "regrouping that reorders rows resets the model")
        expectSessionSurvives(table, "regroup")

        // An expanding paste resets the model, and the very next paste, issued before any deferred
        // visual restoration, still lands on the same stable cell.
        table.pasteFromClipboard()
        compare(resetSpy.count, 4, "an expanding paste resets the model")
        table.pasteFromClipboard()
        compare(table.manager.pasteTextCalls.length, 2)
        for (const call of table.manager.pasteTextCalls) {
            compare(call[0].kind, "threshold")
            compare(call[0].entryId, "a")
            compare(call[0].tierId, "t2")
        }
        expectSessionSurvives(table, "paste expansion")
    }

    function test_rankHeaderIntentSurvivesResetUntilDeleted() {
        const ref = {}
        const tiers = []
        for (let i = 1; i <= 12; ++i) tiers.push("r" + i)
        const table = makeTable(resetDesc(["a", "b"], tiers, {}), {}, ref)
        resetSpy.target = table.tableModel
        resetSpy.clear()

        verify(table.focusCell({tierId: "r1"}))
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_r1").activeFocus)

        // r1 moves to the far, offscreen end of the ladder.
        ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(resetDesc(["a", "b"], tiers.slice().reverse(), {})))
        compare(resetSpy.count, 1)
        tryVerify(() => { const h = ItemLookup.findByObjectName(table, "rankHeader_r1"); return h !== null && h.activeFocus },
                  3000, "the focused rank header is refocused after the ladder resets")
        verify(table.headerFocused)
        compare(table.pasteDestination().kind, "rankHeader")
        compare(table.pasteDestination().tierId, "r1")

        ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(resetDesc(["a", "b"], tiers.slice(1), {})))
        compare(resetSpy.count, 2)
        tryVerify(() => !table.headerFocused, 3000, "a deleted header clears its paste intent")
        compare(table.focusedTierId, "")
        const destination = table.pasteDestination()
        verify(destination === null || destination.kind !== "rankHeader")
    }

    function test_undoRestoresCompleteSelectionAndCurrentCell() {
        const desc = pasteRows()
        desc.rows.push({entryId: "c", cells: {t1: {displayText: "5"}}})
        const table = makeTable(desc, {
            undoStates: [
                {selected: ["a", "b"], current: {entryId: "a", tierId: "t1", columnKind: 3}},
                {selected: ["b"], current: {entryId: "a", tierId: "", columnKind: 2}},
                {selected: ["c"], current: {entryId: "b", tierId: "", columnKind: 0}}
            ],
            undo: function () {
                this.commandLog.push("undo")
                this.undoCalls.push([])
                const state = this.undoStates.shift()
                this.selectedEntryIds = state.selected
                this.currentCell = state.current
                return {ok: true}
            }
        })

        verify(table.focusCell({entryId: "c", tierId: "t2"}))
        table.undo()
        tryVerify(() => isCurrent(table, "a", 3), 3000, "the restored threshold cell is current")
        compare(table.manager.selectedEntryIds.slice().sort(), ["a", "b"], "Undo keeps the restored multi-selection")
        compare(selectedIdsOf(table), ["a", "b"])
        verify(table.thresholdView.activeFocus)

        // A current cell outside the restored selection does not join it.
        table.undo()
        tryVerify(() => isCurrent(table, "a", 2))
        compare(table.manager.selectedEntryIds.slice(), ["b"])
        compare(selectedIdsOf(table), ["b"])

        table.undo()
        tryVerify(() => isCurrent(table, "b", 0))
        compare(table.manager.selectedEntryIds.slice(), ["c"])
        compare(selectedIdsOf(table), ["c"])
        verify(table.identityView.activeFocus)
        compare(table.manager.undoCalls.length, 3)
    }

    // ---- Session commands from an open cell editor ------------------------------------------------

    // Closing the editor from inside its own key or menu handler must not leave a warning about a
    // destroyed editor or menu behind. Process-wide notices (the offscreen font warning, emitted by
    // whichever test renders text first) are not the editor's.
    function sessionCommandTable() {
        failOnWarning(/\.qml|TypeError|ReferenceError/)
        return makeTable(pasteRows(), {
            clipboardReads: 0,
            clipboardText: function () { this.clipboardReads++; return "5\t6\n7\t8" },
            undo: function () {
                this.commandLog.push("undo")
                this.undoCalls.push([])
                this.selectedEntryIds = ["a", "b"]
                this.currentCell = {entryId: "b", tierId: "t2", columnKind: 3}
                return {ok: true}
            }
        })
    }

    function openEditorWith(table, anchor, text) {
        verify(table.focusCell(anchor))
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor !== null)
        table.activeEditor.selectAll()
        typeText(text)
        compare(table.manager.commandLog, [], "the text is still only in the editor")
    }

    // The committed edit must be the only one: an editor destroyed by the command must not commit
    // its text a second time from a deferred destruction callback.
    function settle(table) {
        wait(50)
        verify(table.activeEditor === null, "the command closed the editor")
    }

    function test_editorPasteShortcutUsesSessionPaste() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")

        keySequence(StandardKey.Paste)

        tryVerify(() => table.manager.pasteTextCalls.length === 1)
        settle(table)
        compare(table.manager.commandLog, ["editThresholdText", "pasteText"])
        compare(table.manager.editThresholdTextCalls[0], ["a", "t1", "12oops"])
        const destination = table.manager.pasteTextCalls[0][0]
        compare(destination.kind, "threshold")
        compare(destination.entryId, "a")
        compare(destination.tierId, "t1")
        compare(table.manager.pasteTextCalls[0][1], "5\t6\n7\t8")
        compare(table.manager.clipboardReads, 1)
    }

    function test_editorUndoShortcutUsesSessionUndo() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")

        keySequence(StandardKey.Undo)

        tryVerify(() => table.manager.undoCalls.length === 1)
        settle(table)
        compare(table.manager.commandLog, ["editThresholdText", "undo"])
        compare(table.manager.editThresholdTextCalls[0], ["a", "t1", "12oops"])
        tryVerify(() => isCurrent(table, "b", 4))
        compare(selectedIdsOf(table), ["a", "b"])
    }

    function test_nameEditorsRouteSessionCommands() {
        {
            const table = sessionCommandTable()
            openEditorWith(table, {entryId: "a", columnKind: 2}, "Renamed")
            keySequence(StandardKey.Paste)
            tryVerify(() => table.manager.pasteTextCalls.length === 1)
            settle(table)
            compare(table.manager.commandLog, ["renameScenario", "pasteText"])
            compare(table.manager.renameScenarioCalls[0], ["a", "Renamed"])
            compare(table.manager.pasteTextCalls[0][0].kind, "scenario")
        }
        {
            const table = sessionCommandTable()
            openEditorWith(table, {entryId: "a", columnKind: 0}, "Grp")
            keySequence(StandardKey.Undo)
            tryVerify(() => table.manager.undoCalls.length === 1)
            settle(table)
            compare(table.manager.commandLog, ["renameGroup", "undo"])
            compare(table.manager.renameGroupCalls[0], ["g1", "Grp"])
        }
    }

    function test_escapeDiscardsEditorTextWithoutCommand() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "77")
        keyClick(Qt.Key_Escape)
        tryVerify(() => table.activeEditor === null)
        wait(50)
        compare(table.manager.commandLog, [])
    }

    function editorMenuItem(table, label) {
        const menu = table.activeEditor.ContextMenu.menu
        verify(menu !== null, "the cell editor offers a context menu")
        for (let i = 0; i < menu.count; ++i) {
            const item = menu.itemAt(i)
            if (item && item.text === label) return item
        }
        return null
    }

    function triggerEditorMenu(table, label) {
        mouseClick(table.activeEditor, 5, 5, Qt.RightButton)
        const item = editorMenuItem(table, label)
        verify(item !== null, "the editor menu has " + label)
        if (item.action) item.action.trigger()
        else item.triggered()
    }

    function test_editorMenuPasteUsesSessionPaste() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")

        triggerEditorMenu(table, "Paste")

        tryVerify(() => table.manager.pasteTextCalls.length === 1)
        settle(table)
        compare(table.manager.commandLog, ["editThresholdText", "pasteText"])
        compare(table.manager.editThresholdTextCalls, [["a", "t1", "12oops"]])
        compare(table.manager.pasteTextCalls[0][0].entryId, "a")
        compare(table.manager.clipboardReads, 1)
    }

    function test_editorMenuUndoUsesSessionUndo() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")

        triggerEditorMenu(table, "Undo")

        tryVerify(() => table.manager.undoCalls.length === 1)
        settle(table)
        compare(table.manager.commandLog, ["editThresholdText", "undo"])
        compare(table.manager.editThresholdTextCalls, [["a", "t1", "12oops"]])
    }

    function test_editorMenuCopyStaysLocal() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")
        table.activeEditor.selectAll()

        triggerEditorMenu(table, "Copy")

        wait(50)
        verify(table.activeEditor !== null, "copying leaves the editor open")
        compare(table.activeEditor.text, "12oops")
        compare(table.manager.commandLog, [])
        keyClick(Qt.Key_Escape)
    }

    // ---- Final review regressions -----------------------------------------------------------------

    function openEditorMenu(table) {
        const editor = table.activeEditor
        mouseClick(editor, 5, 5, Qt.RightButton)
        const menu = editor.ContextMenu.menu
        tryVerify(() => menu.opened, 3000, "right-click opens the editor menu")
        return menu
    }

    function clickEditorMenu(table, label) {
        openEditorMenu(table)
        const item = editorMenuItem(table, label)
        verify(item !== null, "the editor menu has " + label)
        mouseMove(item, item.width / 2, item.height / 2)
        mouseClick(item)
    }

    // A real pointer hovers and clicks the item; the editor must survive until the command runs.
    function test_editorMenuPointerPasteAndCopy() {
        {
            const table = sessionCommandTable()
            openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")
            clickEditorMenu(table, "Paste")
            tryVerify(() => table.manager.pasteTextCalls.length === 1)
            settle(table)
            compare(table.manager.commandLog, ["editThresholdText", "pasteText"])
            compare(table.manager.editThresholdTextCalls, [["a", "t1", "12oops"]])
        }
        {
            const table = sessionCommandTable()
            openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")
            table.activeEditor.selectAll()
            clickEditorMenu(table, "Copy")
            wait(50)
            verify(table.activeEditor !== null, "copying with the pointer leaves the editor open")
            compare(table.activeEditor.text, "12oops")
            compare(table.manager.commandLog, [])
            keyClick(Qt.Key_Escape)
        }
    }

    // Escape dismisses an open editor menu; only a second Escape discards the typed text.
    function test_escapeClosesEditorMenuBeforeDiscarding() {
        const table = sessionCommandTable()
        openEditorWith(table, {entryId: "a", tierId: "t1"}, "12oops")
        const menu = openEditorMenu(table)

        keyClick(Qt.Key_Escape)

        tryVerify(() => !menu.opened, 3000, "Escape closes the menu")
        verify(table.activeEditor !== null, "the editor stays open")
        compare(table.activeEditor.text, "12oops")
        keyClick(Qt.Key_Return)
        tryVerify(() => table.activeEditor === null)
        compare(table.manager.commandLog, ["editThresholdText"])
        compare(table.manager.editThresholdTextCalls, [["a", "t1", "12oops"]])
    }

    // Undo reveals the restored cell; a rank header that held focus must not take it back later.
    function test_undoFromRankHeaderEndsOnRestoredCell() {
        const ref = {}
        const table = makeTable(resetDesc(["a", "b", "c"], ["t1", "t2"], {}), {
            undo: function () {
                this.commandLog.push("undo")
                this.undoCalls.push([])
                ref.fixture.setProjection(TestDoubles.benchmarkTableProjection(resetDesc(["c", "b", "a"], ["t2", "t1"], {})))
                this.selectedEntryIds = ["a"]
                this.currentCell = {entryId: "a", tierId: "t1", columnKind: 3}
                return {ok: true}
            }
        }, ref)
        verify(table.focusCell({tierId: "t2"}))
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t2").activeFocus)

        table.undo()
        wait(100)

        verify(table.thresholdView.activeFocus, "the restored cell keeps the focus")
        verify(!table.headerFocused)
        const destination = table.pasteDestination()
        compare(destination.kind, "threshold")
        compare(destination.entryId, "a")
        compare(destination.tierId, "t1")
    }

    // ---- The add controls and horizontal reach ---------------------------------------------

    function tiersNamed(count) {
        const tiers = []
        for (let i = 0; i < count; ++i)
            tiers.push({id: "r" + i, name: "Rank " + i})
        return tiers
    }

    function topOf(item, ancestor) {
        return item.mapToItem(ancestor, 0, 0).y
    }

    function test_addControlsAreLabelledAndSitDirectlyUnderTheLastRow() {
        const table = makeTable({tiers: [{id: "t1", name: "Gold"}], rows: [
            {entryId: "a", name: "Alpha", cells: {t1: {displayText: "1"}}},
            {entryId: "b", name: "Beta", cells: {t1: {displayText: "2"}}}
        ]})
        const category = ItemLookup.findByObjectName(table, "addCategoryPlus")
        const scenario = ItemLookup.findByObjectName(table, "addScenarioPlus")
        verify(category !== null && scenario !== null)

        // A visible label, not a bare glyph, and a tooltip that says what the control does.
        verify(String(category.text).length > 1 && String(scenario.text).length > 1)
        verify(String(category.text).toLowerCase().indexOf("category") !== -1, category.text)
        verify(String(scenario.text).toLowerCase().indexOf("scenario") !== -1, scenario.text)
        verify(String(category.ToolTip.text).length > 0 && String(scenario.ToolTip.text).length > 0)
        verify(String(scenario.ToolTip.text).toLowerCase().indexOf("uncategorized") !== -1)

        // The row of controls begins where the table content ends.
        const lastRow = cellFor(table, "b", "scenario")
        verify(lastRow !== null)
        tryVerify(() => {
            const gap = topOf(category, table) - (topOf(lastRow, table) + lastRow.height)
            return gap >= 0 && gap <= table.rowHeight
        }, 2000, "add-category sits under the last row, not at the bottom of the empty area")
        compare(topOf(scenario, table), topOf(category, table))
        verify(topOf(category, table) < table.height / 2, "the controls are nowhere near the table's bottom edge")
    }

    function test_addControlsStayVisibleBelowALongTable() {
        const table = makeTable(TestDoubles.benchmarkTableLargeDesc(200, 151, 170))
        const category = ItemLookup.findByObjectName(table, "addCategoryPlus")

        tryVerify(() => topOf(category, table) + category.height <= table.height + 0.5,
                  2000, "a table taller than the editor scrolls inside its cap and keeps the controls on screen")
        verify(topOf(category, table) > table.height / 2)
    }

    function test_addControlsSitUnderTheHeaderOfAnEmptyTable() {
        const table = makeTable({tiers: [], rows: []})
        const category = ItemLookup.findByObjectName(table, "addCategoryPlus")

        tryVerify(() => topOf(category, table) <= table.headerHeight + 80)
        verify(category.visible)
    }

    function test_ranksBeyondTheViewportAreSignalledAndReachable() {
        const tiers = tiersNamed(14)
        const table = makeTable({tiers: tiers, rows: [{entryId: "a", name: "Alpha", cells: {}}]})
        const bar = ItemLookup.findByObjectName(table, "thresholdHorizontalBar")
        verify(bar !== null, "the threshold view must expose its horizontal scrollbar")
        const view = table.thresholdView

        verify(view.contentWidth > view.width, "fixture must overflow")
        tryCompare(bar, "visible", true)
        compare(bar.policy, ScrollBar.AlwaysOn, "the scrollbar is not left to fade away")
        verify(bar.size < 1)
        // The bar sits under the body, not over its last row.
        verify(topOf(bar, table) >= topOf(cellFor(table, "a", "scenario"), table) + table.rowHeight - 0.5)

        // Header and body end together: the last visible header is never clipped by the + control.
        const header = ItemLookup.findByObjectName(table, "rankHeader")
        tryVerify(() => Math.abs(header.mapToItem(table, header.width, 0).x
                                 - view.mapToItem(table, view.width, 0).x) < 1.5)

        // Scrolled to the end, the last rank column is there with the body.
        view.contentX = view.contentWidth - view.width
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_r13") !== null)
        const last = ItemLookup.findByObjectName(table, "rankHeader_r13")
        verify(last.mapToItem(table, last.width, 0).x <= view.mapToItem(table, view.width, 0).x + 0.5,
               "the last rank is fully inside the viewport")
    }

    function test_noScrollIndicatorWhenEveryRankFitsAndAddRankFollowsTheLastOne() {
        const table = makeTable({tiers: tiersNamed(3), rows: [{entryId: "a", name: "Alpha", cells: {}}]})
        const bar = ItemLookup.findByObjectName(table, "thresholdHorizontalBar")
        const plus = ItemLookup.findByObjectName(table, "addRankPlus")

        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_r2") !== null)
        compare(bar.visible, false)
        const last = ItemLookup.findByObjectName(table, "rankHeader_r2")
        tryVerify(() => {
            const gap = plus.mapToItem(table, 0, 0).x - last.mapToItem(table, last.width, 0).x
            return gap >= -0.5 && gap <= 4
        }, 2000, "the add-rank control follows the last rank when they all fit")
    }

    function headerLabel(header) {
        for (const child of header.children)
            if (child.hasOwnProperty("truncated") && child.visible) return child
        return null
    }

    // Rank columns share the width the identifying columns leave, equally; when that share would
    // clip a header they keep the widest header's width and the ranks scroll instead.
    function test_rankColumnsShareTheSpareWidthButNeverClipTheirHeaders() {
        const names = ["Iron", "Bronze", "Silver", "Gold", "Platinum"]
        const table = makeTable({tiers: names.map((name, i) => ({id: "t" + i, name: name})),
                                 rows: [{entryId: "a", name: "Alpha", cells: {}}]})
        const bar = ItemLookup.findByObjectName(table, "thresholdHorizontalBar")
        const view = table.thresholdView
        table.width = 1100
        const share = Math.floor((table.width - table.identityTotalWidth - table.rankSlotWidth) / names.length)
        verify(share >= table.rankHeaderMinimumWidth,
               "fixture: the five ranks fit at 1100 (share " + share + ", minimum " + table.rankHeaderMinimumWidth + ")")

        tryCompare(table, "thresholdWidth", share)
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t4") !== null)
        for (let i = 0; i < names.length; ++i) {
            const header = ItemLookup.findByObjectName(table, "rankHeader_t" + i)
            compare(header.width, share, names[i] + " gets an equal share")
            verify(!headerLabel(header).truncated, names[i] + " header is not clipped")
        }
        compare(bar.visible, false, "everything fits: no scrollbar")
        // The add-rank control still follows the last rank.
        const plus = ItemLookup.findByObjectName(table, "addRankPlus")
        const last = ItemLookup.findByObjectName(table, "rankHeader_t4")
        const gap = () => plus.mapToItem(table, 0, 0).x - last.mapToItem(table, last.width, 0).x
        waitForRendering(table)
        verify(gap() >= -0.5 && gap() <= names.length, "the add-rank control follows the last rank, gap " + gap()
               + ", plus x " + plus.mapToItem(table, 0, 0).x + ", header width " + last.width)

        table.width = 600
        tryCompare(table, "thresholdWidth", table.rankHeaderMinimumWidth)
        tryCompare(bar, "visible", true)
        view.contentX = view.contentWidth - view.width
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t4") !== null)
        tryVerify(() => headerLabel(ItemLookup.findByObjectName(table, "rankHeader_t4")) !== null)
        verify(!headerLabel(ItemLookup.findByObjectName(table, "rankHeader_t4")).truncated,
               "the widest header is never clipped")

        // Wide again: the first rank sits right after the identifying columns, header and body.
        table.width = 1100
        tryCompare(table, "thresholdWidth", share)
        tryVerify(() => ItemLookup.findByObjectName(table, "rankHeader_t0") !== null
                        && cellFor(table, "a", "t0") !== null)
        tryVerify(() => Math.abs(ItemLookup.findByObjectName(table, "rankHeader_t0").mapToItem(table, 0, 0).x
                                 - table.identityTotalWidth) < 0.5, 2000, "the first rank header follows the scenario column")
        tryVerify(() => Math.abs(cellFor(table, "a", "t0").mapToItem(table, 0, 0).x - table.identityTotalWidth) < 0.5,
                  2000, "the first rank cell follows the scenario column")
    }

    function test_identifyingColumnsKeepTheirWidthWhenThereAreNoRanks() {
        const table = makeTable({tiers: [], rows: [{entryId: "a", name: "Alpha", cells: {}}]})
        const placeholder = ItemLookup.findByObjectName(table, "emptyRankHeader")

        tryVerify(() => placeholder.visible && placeholder.width > 150)
        const scenario = cellFor(table, "a", "scenario")
        verify(scenario !== null)
        compare(table.identityView.width, table.identityWidths[0] + table.identityWidths[1] + table.identityWidths[2])
    }
}
