---
type: added
area: Benchmarks
user: The Benchmark Manager shows "• Unsaved" next to the benchmark name while the open benchmark has changes you have not saved.
---
`BenchmarkManagerDialog.qml`: an `unsavedLabel` ("• Unsaved", the palette accent at 80% opacity, accessible name "The benchmark has unsaved changes") right after `benchmarkNameField` in the editor header's name row, visible while `benchmarkManagerVm.dirty`. No new state: `BenchmarkManagerViewModel::dirty()` already compares the working copy with its accepted baseline (`!m_baseline || *m_draft != *m_baseline || !m_cellInputs.empty()`), so the label goes on a save (the admitted token becomes the baseline) and when undo returns the draft to that baseline. A never-saved new benchmark has no baseline and stays dirty.
Tests: `tst_BenchmarkManagerDialog::test_unsavedLabelFollowsTheDraftsDirtyState`, `BenchmarkManagerVm.UndoingBackToTheSavedCopyClearsDirty`.
