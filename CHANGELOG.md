# Changelog

The technical record — every change, user-visible or not. For the user-facing subset, see
[RELEASE_NOTES.md](RELEASE_NOTES.md). Pending entries live as fragments in `changelog.d/` and are
assembled into both files at version bump.

## v0.4.0-alpha

Reconstructed from `git log v0.2.0-alpha..v0.4.0-alpha`; predates the `changelog.d/` workflow, so it is
coarser than later sections will be.

### Scenario & run selection

**Added**
- `ScenarioBrowserViewModel` plus a `SelectionPanel.qml` scaffold for search-based scenario selection.
  The panel is purely signal-out — it holds no VM reference; the VM translates QML's
  `(hash, startTimeMs)` pairs into `domain::ScenarioRunId`s for `ISessionController`.
- Run/scenario queries exposed up the layer stack from `domain` through to `app`.
- Scenario search bar and scenario list, run list within a scenario, sorting options for the run list,
  and a "recent runs" quick-access list spanning all scenarios.

**Fixed**
- Two rounds of fixes to the selection interface following early use.

### Graphing

**Changed**
- Rendering pipeline replaced QtGraphs with a custom `GraphCanvas`, for finer control over drawing.
  QtGraphs was deprecated, then removed.
- Monotone cubic interpolation improved via `monotone_spline.h`.
- Broad graphing refactor: axes calculated in their own class and painted separately from the graph;
  series given their own model/painter; hover functionality lifted out of each implementation; raw data
  formatted into UI data by `value_transform`; columns defined outside `GraphViewModel`; new metrics made
  viewable and easier to add via `PerfColumnBuilder`.

**Added**
- `AxisModel`, calculating "nice" tick positions with Heckbert's algorithm, integrated into
  `GraphViewModel`, `PlaytimeGraphViewModel` and `GraphCanvas`.
- Playtime graph, with a shared `GraphViewModelBase` underpinning both graphs.
- Hover tooltip keyed on x-coordinate alone.

### Data & profile cache

**Changed**
- Profile cache handling moved to file-based paths instead of directories.

### Build & packaging

**Changed**
- Version bumped to 0.4.0.

### Architecture

**Added**
- Integration tests driving the real wired object graph, and a component gallery that pairs with
  `qmlpreview` for live previews of QML components.

**Changed**
- Block comments cleaned up across the codebase.
