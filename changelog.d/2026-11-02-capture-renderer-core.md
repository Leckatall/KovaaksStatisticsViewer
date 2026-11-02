---
type: internal
area: Graphing
---
Capture layout core under `src/ui/capture/`: `captureFont` builds unhinted fonts with no subpixel antialiasing from a value description, `captureOutputPixels` holds the one rounding policy, and `validateCaptureOutput` checks an injectable `CaptureLimits` set. All numeric limits are provisional.
`renderCapture` replays a flat `CaptureLayout` item list into a fresh `RasterBuffer` on any thread, painting the graph plot through `paintGraphScene`. It reaches no view-model or QML header, which is why `CaptureFontDescription` sits in its own header.
`AxisPainter::Style::font` and the style overloads of `paintGraphScene` and `graphPlotRect` let capture measure and paint with an explicit font; the live path passes a default style and takes its old branch.
