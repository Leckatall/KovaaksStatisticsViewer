---
type: changed
area: Graphing
user: Scenario-history column visibility and Y-axis choice reset to defaults once, due to an internal settings-key format change.
---
`SeriesModel` is now a QObject owning `id`/`name`/`color`/`column` as `Q_PROPERTY`s, exposed to QML
directly via `GraphViewModel::allSeries`/`CompletionHistoryViewModel::allSeries`
(`QQmlListProperty<SeriesModel>`) instead of a hand-copied `QVariantList`. `GraphViewModelBase`'s
deprecated `columnName`/`columnColor`/`columnKey` callbacks are removed; QML now reads series
name/color directly off `SeriesModel` objects. Settings that persisted by the old `columnKey()`
string now key off `SeriesModel::id` instead.
