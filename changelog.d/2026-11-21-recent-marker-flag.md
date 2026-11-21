---
type: internal
area: Benchmarks
---
The progress grid's recent-average marker takes its position from the domain: `ProgressGridScenarioRow::recentPosition` is set from `ScenarioBenchmarkEvaluation::normalizedRecentRank()` whenever the scenario has runs, so the grid's own `tierScalePosition` is removed.
Whether the marker shows is now an explicit flag instead of a `-1` sentinel: the row map carries `hasRecentMarker` and a plain `recentPosition` (0 when absent), `ProgressGridRow.qml` binds them to `TierBar`'s new `markerShown` and `marker` (which now defaults to 0), and `TierBar`'s label-avoidance, value-crossing and visibility checks read `markerShown`. `table_capture_layout.cpp` drops its `>= 0.0` checks on the marker and on the key's "Recent average" entry.
A played scenario without a usable ladder (only possible in an Incomplete benchmark) now gets a marker at position 0, and its tooltip reads `Recent average X · thresholds incomplete`, matching the row's PB description.
