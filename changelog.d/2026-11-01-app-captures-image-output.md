---
type: internal
area: Architecture
---
`RasterBuffer`/`RasterImage` — a Qt-free raster transport in `ksv_contracts` with checked dimensions and shared immutable storage; `wrapRaster` reads it as `Format_RGB32` without copying and keeps it alive through the `QImage` cleanup function.
`ImageOutputUseCase` — owns global admission of one output at a time, operation ids and a single typed completion delivered through the event loop. Copy publishes on the GUI thread; a save runs on the use case's own one-thread pool, which the destructor joins, so a save finishing during exit reports nothing.
`QtPngImageWriter` writes through `QSaveFile` with direct-write fallback off and sets no image text, so a failed save leaves the destination intact. `QtClipboardImageWriter` reports success only after confirming ownership, behind a `Backend` seam so tests never reach the system clipboard; the default backend is not exercised yet.
