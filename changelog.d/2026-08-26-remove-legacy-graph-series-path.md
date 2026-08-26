---
type: removed
area: Graphing
---
Deleted `GraphViewModel::fetchData()`'s deprecated legacy fallback branch (the
`TODO(2026-08-19)` one) and the whole `GraphSeries`/`PerfColumnBuilder`/`ColumnId` pipeline it was
the last production caller of.
`IGraphUseCase`/`GraphUseCase` lose `get_series()`; deleted `src/app/usecases/perf_column_builder.{h,cpp}`,
`src/app/contracts/graph_series.h`, `src/app/contracts/graph_column.h`, and their dedicated tests
(`tests/app/perf_column_builder_test.cpp`, `tests/app/graph_column_test.cpp`, the `GetSeries*`
cases in `graph_use_case_test.cpp`).
`tests/ui/graph_vm_test.cpp` migrated its ~9 legacy-path tests to build `ResolvedGraph` fixtures via
`defaultSeriesConfigs()` instead of `GraphSeries`/`ColumnId`; `FetchDataDefaultsMissingTrailingValuesToZero`
had no equivalent in the resolved-graph model and was dropped instead of migrated.
Also fixed a latent bug the migration exposed: `fetchData()`'s resolved-graph branch built each
series' `SeriesModel::points` but never wrote `rows[i][seriesId]`, so `GraphViewModel::seriesPoints()`
(unused by QML, which reads via `series()`/`SeriesModel` instead) returned an all-zero `y()` for every
non-time column in production since the SeriesConfig migration.
