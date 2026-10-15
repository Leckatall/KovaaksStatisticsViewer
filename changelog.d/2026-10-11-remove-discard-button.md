---
type: removed
area: Benchmarks
user: The Benchmark Manager no longer has an X button that threw away your unsaved changes in one click; closing the Manager or opening another benchmark asks whether to save or discard them.
---
`BenchmarkManagerDialog.qml`: removed the editor toolbar's `discardButton` (X icon, "Discard unsaved changes and close the editor"), which called `BenchmarkManagerViewModel::discard()` directly and bypassed the `dirtyClosePrompt`. Leaving a dirty draft now always goes through that prompt: closing the Manager window, opening another library entry, or starting a new benchmark. `discard()` stays: the prompt's Discard choice uses it.
`src/ui/qml/icons/x.svg` was used only by that button and is removed with its `src/ui/CMakeLists.txt` entry.
Tests (`tst_BenchmarkManagerDialog`): new `test_editorHasNoDiscardButton`; `test_staleBaselineWarning` now checks that closing still offers Discard through the prompt; `test_benchmarkNameKeepsItsWidthWhenTheEditorIsNarrow` and `test_editorActionsWrapAllOrNothing` check the remaining undo/paste/save buttons. Opening another entry with a dirty draft was already covered by the existing transition tests. No tour referenced the button.
