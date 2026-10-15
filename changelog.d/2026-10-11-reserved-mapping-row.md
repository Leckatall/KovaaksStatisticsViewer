---
type: changed
area: Benchmarks
user: In the Benchmark Manager the scenario mapping row is always shown (with a hint until you select a scenario), so selecting a cell no longer pushes the table down.
---
`BenchmarkManagerDialog.qml`: the `mappingPanel` Pane was `visible: root.currentEntryId !== ""`, so selecting any cell of a row inserted a 32px row (plus 6px spacing) above the table and moved it down 38px (y 40 → 78 in the driver). The Pane is now always laid out. With no current row it shows `mappingHint` ("Select a scenario to see its mapping", placeholder colour, as tall as the mapping buttons); with one, the existing "Mapping for …" label, state and Change/Clear buttons. The table sits at y 78 either way.
The candidate list below that row is unchanged: a row that has scenario candidates (an ambiguous or auto-mappable entry, or a resolved one with recorded candidates) still lists them under the row, which grows the panel for that row.
Test: `tst_BenchmarkTableManagement::test_selectingAScenarioNeverMovesTheTable`.
