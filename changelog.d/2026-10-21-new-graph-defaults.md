---
type: changed
area: Graphing
user: New default graph settings. The run graph now shows your score, your projected final score compared with your top and recent runs, and running accuracy, and the other graphs open on a recent range instead of your whole history. Existing settings are kept; use "Restore default lines" in Graph Lines to switch to the new lineup. The benchmark playtime graph is now titled the same way as the daily playtime graph.
---
`defaultSeriesConfigs()`/`defaultAxisConfigs()` replaced: 8 computed lines (4 on), primitive lines dropped, axes Score/Accuracy; widths 3.0 to match what `SeriesPainter` already draws.
`kFirstUserComputedSeriesId`/`kFirstUserAxisId` removed; `defaultRunSeriesDocument()` derives its allocators from the default lists (ids 1–9 were never reserved after the base/computed split was removed).
`migrateV1ToV2` now carries a frozen copy of the pre-change axes instead of reading `defaultAxisConfigs()`; `seededRunSeries` seeds the new lineup and maps legacy disabled-column keys by series id, not list position.
`GraphDescriptor::defaultRange` gives each graph its own default range (playtime 30 days, benchmark rank 90 days, both anchored; scenario history latest 100 runs); `kDefaultAveragingWindowDays` 3 → 7.
`SettingsViewModel::restoreDefaultGraphLines()` stages the default document and clears the axis-label choice; button in `SeriesConfigDraftPanel.qml`.
`BenchmarkHistoryViewModel::metricName()` returns `PlaytimeGraphViewModel::titleFor()` for playtime, replacing its own wording whose spelled-out "Three-day" case existed only because 3 was the default window.
