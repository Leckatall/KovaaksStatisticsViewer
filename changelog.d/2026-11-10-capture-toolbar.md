---
type: changed
area: User Interface
user: Graph and benchmark captures now have separate Copy, Save and Preview buttons. Save goes straight to the file picker at the default size; Preview opens the adjustable export window.
---
`CaptureActions` is now a `Control` (`Accessible.ToolBar`) holding three `ChromeIconButton`s (`captureCopy_<key>`, `captureSave_<key>`, `capturePreview_<key>`) inside a 1 px outline box and separated by short 1 px dividers; the camera button, the chevron, its `Menu`, the `menu` alias, `captureMenu_`/`captureExport_` and `icons/camera.svg` are gone.
Direct save: `CaptureCoordinator::quickSave` snapshots the content and emits `saveDestinationRequested(suggestedFileName)` without rendering; `confirmQuickSave(path)` renders at the default options and hands the raster to `savePng`, `cancelQuickSave()` drops the snapshot. While a destination is pending, quick copy, preview and another quick save are refused with a busy notice. `CaptureViewModel` exposes `quickSave`/`confirmQuickSave(QUrl)`/`cancelQuickSave` and forwards the signal.
The suggested file name is shared between the preview and the quick save through a file-local `fileNameFor`.
New `shell/CaptureFileDialog.qml` (PNG save `FileDialog` with `openFor(fileName)`) backs both the preview's `captureSaveDialog` and `Main.qml`'s `captureQuickSaveDialog`.
On `previewClosed` focus returns to the originating Preview button.
