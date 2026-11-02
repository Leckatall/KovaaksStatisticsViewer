---
type: internal
area: User Interface
---
`CaptureActions` (camera and dropdown) on all six graph panels and the benchmark header, `CaptureStatusStrip` beside import status in `ProfileStatusBar` and mirrored in the preview footer, and window-modal `CapturePreviewDialog` over `CapturePreviewItem`.
`GraphCanvasWithTooltip.graphCanvas` lets a panel reach its loaded canvas from outside the `Loader`.
`captureVm` is threaded down from `Main` as a nullable property, so capture actions are hidden without it.
On `previewClosed` the opening `CaptureActions` retakes focus, and `Main` then moves focus to the current workspace if it is not already inside it.
