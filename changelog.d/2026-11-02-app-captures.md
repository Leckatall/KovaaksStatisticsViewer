---
type: added
area: Graphing
user: A camera button beside each graph and the benchmark table copies a clean, freshly rendered image of it to the clipboard. Its menu also offers Export image…, where you can change the resolution or size, preview the result, copy it, or save it as a PNG.
---
App captures, end to end: `CaptureViewModel` over `CaptureCoordinator` and `CaptureFeedback`, the single-thread render queue, the `ImageOutputUseCase` clipboard and PNG adapters, the camera entry points, the preview dialog and the shared status-bar feedback.
Resource limits adopted at checkpoint C3 live in `standardCaptureLimits()` (edge 16,384 px, area 100 Mpx, estimate 1.5 GiB as a backstop, 2,000,000 source points at 72 bytes each, 16 MiB thumbnail) with the 960 × 600 standard graph reference in `standardCaptureComposition()`.
`CaptureRejection::limit` carries the violated figure, so rejection text states it in the current locale.
The clipboard publisher is still the Qt candidate; choosing between it and the documented fallbacks waits on interactive Windows clipboard runs.
