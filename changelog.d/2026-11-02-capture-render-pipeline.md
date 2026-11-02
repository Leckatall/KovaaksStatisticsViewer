---
type: internal
area: Graphing
---
`IImageOutputUseCase::activeOutput()`, plus the capture render pipeline: `makeCaptureRenderFunction` measures, validates and paints off the live objects, and `CaptureRenderQueue` runs one request on its own thread with one pending slot.
