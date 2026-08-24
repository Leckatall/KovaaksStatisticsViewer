---
type: added
area: Graphing
user: Custom graph lines can now share a Y-axis with each other and with built-in series, instead of always getting their own independent axis.
---
`AxisConfig`/`AxisId` added in `series_config.h`; `SeriesConfig` gains `yAxisId` (reference) and `transformKind` (own value scaling/formatting, replacing the previous hardcoded Accuracy-only special-case).
Replaces `GraphViewModel`'s hardcoded `yAxisFor()` switch/`kYAxisMeta` table with grouping by `yAxisId`, resolved through the existing `axisForSeries()` helper.
Persisted in `SeriesConfigStore`'s JSON document, schema bumped to v2 with an in-memory v1→v2 migration (existing computed series and the Score Total/Expected Final Score/(5s) grouping carry over unchanged).
New axis picker (existing axis, or create one) in `SeriesConfigDraftPanel.qml`, wired through `SettingsViewModel::getAllAxes`/`createAxis`/`updateSeriesAxis`.
