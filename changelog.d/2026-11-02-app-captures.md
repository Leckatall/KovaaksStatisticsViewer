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
`renderCapture` and `paintGraphScene` take the flag as a `CancellationCheck` (`components/cancellation_check.h`, a `std::function<bool()>`), so tests can cancel partway through a paint.
`CaptureFeedback` keeps a failure until it is dismissed or replaced by a new failure; a success or notice shows over it for its usual lifetime and the failure then returns.
An empty or non-local export destination, or a refused save admission, keeps the preview and its image.
`CaptureViewModel` exposes `previewImageWidth`/`previewImageHeight`, because QML cannot read a `QImage`'s size and the preview would otherwise never appear.
Escape is a window-level `Shortcut`, so it closes the preview from the footer buttons and after Copy.
The options area scrolls and the image keeps a minimum height, so the footer, image and options stay reachable at the minimum window size and larger text.
While the preview is open only its own status strip announces results to screen readers (`CaptureStatusStrip.announces`), and the preview's unavailable message has an alert role.
The preview's image scroller shows a focus ring when it has keyboard focus.
Closing the preview returns focus to the export dropdown that opened it, not the quick-copy camera.
Tabbing to a clipped option (Keep aspect ratio, Width, Height) scrolls it into view in the options area.
The resolution marks line up with the slider handle at every value, and the collapsed Advanced options button shows a right-pointing chevron.
The width and height fields stay disabled until `CaptureViewModel::hasReference` (the first render outcome) so an early entry is not dropped silently.
