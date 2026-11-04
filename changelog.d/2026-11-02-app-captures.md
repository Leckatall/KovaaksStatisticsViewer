---
type: added
area: Graphing
user: A camera button beside each graph and the benchmark table copies a clean, freshly rendered image of it to the clipboard. Its menu also offers Export image…, where you can change the resolution or size, preview the result, copy it, or save it as a PNG.
---
App captures, end to end: `CaptureViewModel` over `CaptureCoordinator` and `CaptureFeedback`, the single-thread render queue, the `ImageOutputUseCase` clipboard and PNG adapters, the camera entry points, the preview dialog and the shared status-bar feedback.
Resource limits adopted at checkpoint C3 live in `standardCaptureLimits()` (edge 16,384 px, area 100 Mpx, estimate 1.5 GiB as a backstop, 2,000,000 source points at 72 bytes each, 16 MiB thumbnail) with the 960 × 600 standard graph reference in `standardCaptureComposition()`.
`CaptureRejection::limit` carries the violated figure, so rejection text states it in the current locale.
The clipboard publisher is still the Qt candidate; choosing between it and the documented fallbacks waits on interactive Windows clipboard runs.
Renders cancel cooperatively: each `CaptureRenderRequest` carries a shared atomic flag, checked between painted items and between series in `paintGraphScene`; `CaptureRenderQueue::cancel(session)` (called from `closePreview`) and the queue's destructor set it, and a cancelled render is never delivered. Exit therefore does not wait for a dense render, while an accepted save still finishes.
`CaptureFeedback` keeps a failure until it is dismissed or replaced by a new failure; a success or notice shows over it for its usual lifetime and the failure then returns.
An empty or non-local export destination, or a refused save admission, keeps the preview and its image.
