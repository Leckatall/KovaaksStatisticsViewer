---
type: changed
area: Benchmarks
user: The benchmark scenario table's PB and Recent columns, now shown on each progress bar, are replaced by the date you set your personal best and how many times you have played the scenario.
---
`domain::ScenarioProjection` gains `personalBestTime` (`std::optional<sys_seconds>`): `benchmark_evaluator.cpp` now tracks the best `RunFact` rather than just its score, taking the earliest `start_time` among runs tied on the PB score, so the date is when the PB was first set and does not depend on fact order.
`BenchmarkProgressGridModel` rows drop `recentText` (and its sample-count helper) and gain `personalBestDateText` (local day, `yyyy-MM-dd`, the format the Benchmark Manager uses for candidate dates) and `playCountText` (`runCount`, locale-grouped); both are "—" for unplayed or unmatched entries. `personalBestText` stays, for the tier bar's PB label.
`BenchmarkProgressGrid.qml`: columns `pb`/`recent` become `pbDate`/`plays` (headers "PB date"/"Plays", cells `progressRow_<id>_personalBestDate`/`_playCount`). The recent average is still on the bar as its marker, with its tooltip.
Tests: new `BenchmarkEvaluator.PersonalBestTimeIsTheEarliestRunThatScoredIt` and `BenchmarkProgressGridModel.RowsCarryThePersonalBestDateAndPlayCount`; the QML grid tests and `TestDoubles.js` fixtures use the new keys and object names.
