---
type: internal
area: Graphing
---
Removed `IGraphLineConfig` (`src/data/interfaces/`), `qt_data::GraphLineConfig`, and the
`GraphColumnPreferences` use case + `IGraphColumnPreferences` contract — all superseded by
`ISeriesConfigStore` with zero remaining callers.
Removed the empty `SeriesConfigValidator` class from `series_config.h`.
Removed unused `GraphCanvas::m_hiddenSeriesIds` and `BuildContext::perf` (`perf_column_builder.cpp`).
Deleted the composition-root test asserting `App` is unconstructible with the legacy
`IGraphLineConfig` param, now meaningless with the type gone.
Fixed `AGENTS.md` documenting `IGraphLineConfig` as App's third injected leaf; it's `ISeriesConfigStore`.
