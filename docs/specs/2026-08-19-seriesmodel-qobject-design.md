# SeriesModel as a QObject; retiring columnName/columnColor/columnKey

## Context

`GraphViewModelBase` (and its three implementations — `GraphViewModel`,
`CompletionHistoryViewModel`, `PlaytimeGraphViewModel`) expose per-column
display data through three `[[deprecated]]` callback methods:
`columnName(int)`, `columnColor(int)`, `columnKey(int)`. QML calls back into
the VM with an integer column to resolve a series' name/color, and separately
uses `columnKey(int)` as a persistence key for QSettings-backed UI state
(column visibility, selected Y axis).

`SeriesModel` (`src/ui/presentation/series_model.h`) already carries `name`
and `color` as plain fields, populated by `GraphViewModel::fetchMetadata()`
from domain `SeriesConfig`/`SeriesPresentation`. It's a plain struct, not a
QObject, and QML never sees it directly — QML only sees a hand-built
`QVariantList allSeries` projection. The `columnName`/`columnColor` callback
pattern duplicates data that already lives on `SeriesModel`, and previously
duplicated a *second* static table that could drift from `SeriesConfig`
(fixed in commit `5ca78dc`, per the changelog).

The `Key` vs `Name` distinction that motivated `columnKey()` is inconsistent
across VMs: in `GraphViewModel`, `columnKey()` just delegates to
`columnName()` — no distinction exists. In `PlaytimeGraphViewModel`, `Key`
returns a stable lowercase string ("playtime", "date") distinct from the
display `Name` ("Playtime (3-day avg)"), used as a QSettings dictionary key.
This inconsistency, plus `SeriesModel` still routing name/color through VM
callbacks instead of owning them, is what this change fixes.

**Outcome:** `SeriesModel` becomes a QObject that owns its own `name`/`color`/
`id`, exposed to QML directly as a list of objects. `columnName`/
`columnColor`/`columnKey` are deleted from `GraphViewModelBase` and all three
VMs. Persistence keying switches from ad hoc strings to the series' numeric
id.

## SeriesModel becomes a QObject

`SeriesModel` gains `Q_OBJECT` and `QML_ELEMENT` (uncreatable — VMs are the
only constructors), with `Q_PROPERTY` for:

- `name` (`QString`)
- `color` (`QColor`)
- `id` (numeric identifier — `SeriesId::value` for `GraphViewModel`; each
  VM's own column enum value for `CompletionHistoryViewModel` and
  `PlaytimeGraphViewModel`, since those aren't domain-`SeriesId`-backed)

`column`, `transform`, `xAxis`/`yAxis`, and `points` remain plain C++
accessors (no `Q_PROPERTY`) — `GraphCanvas`, the only consumer of those
fields, is C++-only.

QObjects aren't copyable, so VM storage changes from value containers
(`QMap<QString, SeriesModel>`, `QList<SeriesModel>`) to VM-owned,
VM-parented `SeriesModel*` instances. Per-refresh lifetime: **rebuild
wholesale on each fetch**, matching today's behavior — no in-place
diffing/updating of existing objects. QML bindings to a specific
`SeriesModel*` are expected to re-resolve after each refresh, same as the
existing `allSeries` re-assignment does today.

## QML consumes SeriesModel directly

`allSeries` (`QVariantList`) and `enabledSeriesIds` (`QVariantList`) are
replaced by a single `QQmlListProperty<SeriesModel>`-typed property exposing
the owned `SeriesModel*` list. QML iterates it and reads `.name`/`.color`/
`.id`/`.enabled` directly instead of receiving a hand-copied variant map.

Call sites that resolve a column via `vm.columnName(modelData)`/
`vm.columnKey(modelData)` switch to iterating the series list and reading
properties off the matching `SeriesModel`:

- `src/ui/qml/AppMenuBar.qml` (menu item labels, settings-key lookups)
- `src/ui/qml/DashboardGraphCanvas.qml` (Y-axis label)
- `src/ui/qml/ScenarioHistoryPanel.qml` (settings-key lookups)

## columnName/columnColor/columnKey are deleted

Not deprecated further — deleted outright:

- `GraphViewModelBase::columnName`, `::columnColor`, `::columnKey` (pure
  virtuals) are removed from the interface.
- The corresponding overrides in `GraphViewModel`, `CompletionHistoryViewModel`,
  and `PlaytimeGraphViewModel` are removed.

Other deprecated `GraphViewModelBase` members (`plottableColumns()`, the
legacy `get_series()`/`GraphSeries` fallback path in `fetchData()`) are
**out of scope** for this change — they're a separate concern and are left
untouched.

## Persistence keys by id, not by ad hoc string

QSettings-backed UI state that was keyed by `columnKey()`'s string (
`VisualSettingsManager`'s `columnVisibility` map, `yAxisColumnKey`, and
equivalents in `AppMenuBar.qml`/`ScenarioHistoryPanel.qml`) switches to
keying by the series' numeric `id`, stringified. `GraphViewModel` already
does this in practice (`m_seriesById` is keyed by `QString::number(SeriesId
::value)`); `CompletionHistoryViewModel` and `PlaytimeGraphViewModel` move
from their hand-picked lowercase strings ("date", "playtime", "score", ...)
to `QString::number(<their column enum value>)`.

This is a deliberate breaking change to already-persisted settings: a user's
saved column-visibility/Y-axis choices under the old string keys will not
match after upgrade and will silently reset to defaults. No migration is
planned — accepted as fine for this project's scale.

## Testing

- `tests/ui/graph_vm_test.cpp`, `completion_history_vm_test.cpp`,
  `playtime_graph_vm_test.cpp`: rewrite construction of `SeriesModel` values
  and calls to `columnName`/`columnColor`/`columnKey` against the new
  QObject-list API.
- `src/ui/components/graph_canvas.cpp` (and its tests, e.g.
  `tests/integration/graph_canvas_geometry_test.cpp`): update iteration over
  series from value access to pointer/reference access.
- No new QML integration test planned beyond updating whatever currently
  exercises `allSeries`/`columnKey` in QML tests, unless implementation
  turns up more.
