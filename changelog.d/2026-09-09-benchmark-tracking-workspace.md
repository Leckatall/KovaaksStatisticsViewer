---
type: added
area: Benchmarks
user: The main window is now split into Scenarios and Benchmarks workspaces, with the Benchmarks tab showing the selected benchmark's rank status, next-tier blockers, rank and playtime history, and a browsable scenario breakdown. Everything on it reads one coherent snapshot, so status, definition, and library choices always reflect the same moment. The Benchmark Manager now shows each scenario's matching state, lets you add a scenario you have already played straight from your profile (equal names kept apart by hash), pick which played scenario an ambiguous entry maps to, and change or clear a mapping without losing its thresholds or placement; a banner flags when automatic resolution could not be written.
---
`BenchmarkWorkspaceSnapshot`, `BenchmarkChoice`, and `BenchmarkChoiceClassification` in
`src/app/contracts/benchmark_workspace_snapshot.h`. `IBenchmarkTrackingUseCase` gains
`snapshot()`; `BenchmarkTrackingUseCase` retains one snapshot and rebuilds it on construction and
on every library/profile change, mapping each library entry to a classified, deterministically
ordered choice and publishing the selected definition, completeness issues, and projection in one
revision.

`BenchmarkTrackingViewModel` (`src/ui/presentation/benchmark_tracking_vm.{h,cpp}`) adapts that one
snapshot into the workspace's selector, explicit state, status summary, blockers, and three owned
presentation-only child models built and installed before any Qt notification:
`BenchmarkHistoryViewModel` (`benchmark_history_vm.{h,cpp}`, a `GraphViewModelBase` reused for the
personal-best average-rank and three-day rolling-playtime lines with aligned UTC-day X bounds and
separate Y axes) and `BenchmarkBreakdownModel` (`benchmark_breakdown_model.{h,cpp}`, a read-only
`QAbstractItemModel` reconstructing the Uncategorized/category/subcategory/scenario tree keyed by
stable element IDs and owning expansion state).

`IBenchmarkManagerUseCase` / `BenchmarkManagerUseCase`
(`src/app/{contracts/i_benchmark_manager_use_case.h,usecases/benchmark_manager_use_case.{h,cpp}}`)
become the manager dialog's sole application boundary, adapting the library service's separate draft
and library publications into one `BenchmarkManagerState` per notification; constructed and retained
in `App::App()` with an `App::benchmarkManagerUseCase()` accessor. `BenchmarkManagerViewModel`
(`src/ui/presentation/benchmark_manager_vm.{h,cpp}`) drops its `IBenchmarkLibraryService`
dependency, takes the use case alone, and rebuilds every library/draft/tree/catalogue/diagnostic
projection from a single `state()` read before emitting. `BenchmarkManagerDialog.qml` gains the
catalogue-backed known-scenario picker beside the retained free-text add path, per-row match-state
labels, ambiguity candidate rows with hash/run-count/last-played detail, Change/Clear mapping
routed through the existing retrospective-reinterpretation warning for saved drafts, and the
non-modal automatic-resolution failure banner.
