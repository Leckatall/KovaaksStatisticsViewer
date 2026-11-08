---
status: accepted
date: 2026-11-04
retrospective: true
---

# 0007: Own capture snapshots and rendering in presentation, image output in the application layer, crossing only as owned values

## Context

App captures lets a user copy or save a freshly rendered image of any of the six graphs or the benchmark tracking table. The content comes from live presentation state: `GraphCanvas`, the graph view models and the benchmark presentation. Imports and live updates keep changing that state while a capture is reviewed, rendered and written.

Forces, from the feature's requirements and design:

- Rendering runs off the GUI thread, so it must not read live QML items, mutable QObjects, or callables that close over live model state.
- A preview fixes its content for the visit, independently of later imports, selection changes or configuration changes.
- An output outlives its preview: confirming a PNG destination closes the preview and the save completes in the background, reporting its outcome in the main status bar.
- Success is reported only after the clipboard publication or file commit has completed, never on admission.
- Clipboard and file work are platform operations that routine tests must not perform.

The choice was made in the feature's design, which was approved on 2026-11-01 before this record, and is implemented on the `app-captures` branch.

## Decision

Capture content crosses layer and thread boundaries only as owned, immutable values. Presentation owns the snapshot and its rendering; the application layer owns output operations.

- **Presentation** reads the target's current state once, on the GUI thread, into an owned capture snapshot that holds no pointer, QObject or callable. Presentation owns the preview session, layout and rendering. Render workers receive only that snapshot through shared const ownership and produce an immutable raster; they never call live items or view models.
- **The application layer** owns output through `IImageOutputUseCase`, a Qt-free contract that accepts a `RasterImage`: immutable, shared, tightly packed, opaque 32-bit pixels. It admits one output operation at a time, identifies each admitted operation, and publishes exactly one typed result for it after the platform work completes. Output operations do not depend on the preview's lifetime.
- **Platform work** sits behind data-layer ports, `IClipboardImageWriter` and `IPngImageWriter`, with Qt implementations in the Qt data layer. `App` constructs every concrete implementation and injects the output leaves explicitly.

This decision governs ownership and the boundary between capture content and output. It does not govern how images are rendered, the graph scene shared with the live canvas, the export table layout, the resource limit values, or which clipboard adapter implementation is used.

## Consequences

- Output admission, completion and failure behaviour can be tested with fake output leaves; routine tests never touch the real clipboard or user files.
- A background save is independent of the preview, so Export can close the preview on confirmation and still report the save's outcome.
- Render workers cannot race live state, because they hold no reference to it.
- Each capture copies the target's data into the snapshot. Measured cost was 4.5 ms for 800,000 graph points, but it grows with data size.
- `App`'s injected constructor requires the two output leaves, so every injected call site (integration tests, the gallery and `ksv_driver`) must supply them.
- Any later export feature has a precedent: move presentation state across threads and layers as owned values, and keep application contracts free of QML and view models.

## Alternatives considered

- **Exporting a bitmap of the live widget.** Not selected: a grab of the live item tree cannot render text and lines freshly at the requested output resolution, and enlarging it does not add detail.
- **Tying output to the preview by locking it until output finishes.** Not selected: Export closes the preview on destination confirmation, and Copy keeps the preview usable, so output operations must outlive and stay independent of the dialog.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [ADR-0004: Route view-model interactions through use cases](0004-route-view-model-interactions-through-use-cases.md)
- [ADR-0005: Own accepted benchmarks in a data service, editing in presentation, and profile reconciliation in the application layer](0005-data-owned-benchmark-service-and-presentation-drafts.md)
- [App captures design](../../features/app-captures/design.md)
- [App captures requirements](../../features/app-captures/requirements.md)
