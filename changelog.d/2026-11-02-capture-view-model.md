---
type: internal
area: Graphing
---
`CaptureViewModel` — new QML-facing facade over `CaptureCoordinator` and `CaptureFeedback`, built in `App::App()` from the provisional limits and supplied to `Main.qml` as the required `captureVm` property. `Main` hands it the theme; keys resolve to the capture sources; `suggestedFileName` is derived from the snapshot title.
