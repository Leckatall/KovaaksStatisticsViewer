---
type: internal
area: Benchmarks
---
Tests now pin the benchmark selector's "unsupported" label, which no test or fixture covered before.
A benchmark file classifies as unsupported when `BenchmarkStore::classifyBytes` (`src/qt_data/benchmark_store.cpp`) reads an integer `schemaVersion` greater than the store's `kSchemaVersion` (1); a non-integer, missing, lower or otherwise mismatched version is Invalid instead. The store keeps the file's `name`/`id` as best-effort display fields, `BenchmarkTrackingUseCase::choiceFor` maps `BenchmarkFileProblem::Unsupported` to a non-selectable `BenchmarkChoiceClassification::Unsupported`, and `BenchmarkTrackingViewModel` emits it as the lowercase string "unsupported".
`tst_BenchmarkWorkspace.qml` gains the data-driven `test_selectorLabelsEveryExceptionKind` (incomplete, invalid, unsupported, using the real VM's lowercase strings): each row's pill must be visible, carry its kind, have a size and be reserved for by the delegate's rightPadding; a mutation hiding the unsupported pill fails it.
`BenchmarkTrackingViewModel.SelectorOrdersLoadedEntriesFirstThenProblemsWithNameAndFilenameSort` now also asserts each entry's classification string, including "unsupported".
Seen in the driver with a schemaVersion 2 overlay benchmark beside the incomplete and invalid fixtures: the red "unsupported" pill renders readably both on a plain (disabled) row and on a forced-highlighted row; no QML change was needed.
