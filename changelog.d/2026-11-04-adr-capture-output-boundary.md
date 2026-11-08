---
type: internal
area: Architecture
---
ADR 0007 records, retrospectively, the app captures boundary: presentation owns the value snapshot, preview session and rendering, with render workers touching only owned immutable values; the application layer owns output operations through the Qt-free `IImageOutputUseCase` and `RasterImage`, with clipboard and PNG work behind `IClipboardImageWriter` and `IPngImageWriter`.
