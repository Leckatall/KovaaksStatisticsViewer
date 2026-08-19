---
type: fixed
area: Graphing
user: Several graph lines (Kills, Dmg, Score Total, Expected Final Score, and the "(5s)" variant) were rendering under the wrong name/color or not at all.
---
`GraphViewModel` derived each series' plot-slot `column` as `displayPosition + Score` against a fixed
legacy `enum Column` sized for the pre-`SeriesConfig` 8-metric set. Once `Hits` got its own
`SeriesConfig` entry, every later built-in series' `displayPosition` shifted by one, so
`ExpectedFinalScoreRecent` computed an out-of-range column and rendered nothing, while `Kills`/`Dmg`/
`ScoreTotal`/`ExpectedFinalScore` each rendered under the next series' name/color/axis.
`column` is now always the series' own `SeriesId::value`, never a position, and `GraphViewModel::Column`
is removed entirely (it had no meaning outside the class). `columnName`/`columnColor` now read from
`m_seriesById` instead of a static table that duplicated (and could drift from) `SeriesConfig`'s own
presentation data; `columnKey` now just delegates to `columnName`. Y-axis grouping is still hardcoded,
but keyed by `SeriesId` instead of column position, and shrunk to only the two cases that actually need
it (`Accuracy`'s percentage transform, the Score-family shared axis) — everything else (including any
future user-created computed series) falls through to `SeriesModel::deriveYAxis()`'s existing per-series
fallback in `GraphCanvas`. Removed `allColumns()`/`columnYAxis()` (zero real callers); gutted
`axisTicks()`/`axisBounds()` to just the Time axis for the same reason, since they can't be deleted
outright without touching `GraphViewModelBase` and the two other view models that implement it.
Marked the legacy `get_series()`/`GraphSeries` fallback branch in `fetchData()` and the orphaned
`GraphColumnPreferences` as deprecated/unwanted — kept working (existing tests still exercise them) but
flagged for removal.
