---
type: fixed
area: Settings
user: Fixed a bug where reordering a graph line by dragging it could throw an error and leave the
  series list rendering rows on top of each other until you dragged something else.
---
`SeriesConfigDraftPanel.qml`'s `DragHandler.onActiveChanged` release branch called
`settingsVm.reorderSeries()` and then reset `draggedSeriesId`/`dragOriginIndex`/`dragPreviewIndex`/
`dragTranslationY` afterward. `reorderSeries()` synchronously emits `seriesConfigurationChanged`,
which reassigns `allSeriesConfigs` -> `sourceRows` -> `displayRows`, causing the `Repeater` (a plain
JS-array model, no incremental diffing) to destroy and recreate every delegate, including the one
whose own handler was still executing. Resuming that handler after the call threw
`ReferenceError: root is not defined`, aborting before the state-reset lines ran and leaving stale
drag-state that `previewOffset()` used to compute Translate offsets for the (fully rebuilt) rows.
Fix: capture the row id and target position into locals, reset all drag state first, and call
`reorderSeries()` last, so nothing after it depends on the delegate's context surviving the call.
Regression test: `test_reentrantModelResetDuringDragReleaseLeavesCleanDragState` in
`tst_SeriesConfigDraftPanel.qml`, using a new `QtObject`-based `reentrantSettingsVmComponent` fake
whose `allSeriesConfigs` is a real observable `property var` (unlike the existing plain-JS-object
fake), so it reproduces the real VM's synchronous NOTIFY-driven model reset.
