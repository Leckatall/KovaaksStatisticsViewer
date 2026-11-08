---
type: internal
area: User Interface
---
`CapturePreviewDialog.qml`'s `onClosing` refused every close and handed it to `closePreview()`, so the view model alone decides dismissal. Qt 6 quits by sending a close to every window the app has shown, hidden ones included, and a single refusal cancels the quit. Once the preview had been opened and dismissed, closing the main window hid it but the process kept running. `onClosing` now refuses only while `previewOpen`.
