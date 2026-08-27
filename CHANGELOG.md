# Changelog

The technical record — every change, user-visible or not. For the user-facing subset, see
[RELEASE_NOTES.md](RELEASE_NOTES.md). Pending entries live as fragments in `changelog.d/` and are
assembled into both files at version bump.

## v0.6.0-beta

### Added

#### Graphing
- Series colors gain an adjustable alpha channel.
- Custom graph lines can share a Y-axis with each other and with built-in series instead of always getting their own; new axis picker in the series-config panel, backed by `AxisConfig`/`AxisId` and a v2 schema migration that carries existing groupings over unchanged.
- Visual expression-tree editor for building and editing graph-line expressions, plus in-place series CRUD (reorder, rename, recolor, re-express, add, delete). Edits preview as a draft and apply only on confirm.
- Graph-series settings persist across launches: versioned series-configuration store with computed-ID allocation, legacy visibility migration, and corrupt-settings quarantine/recovery.
- Foundations for historical average graph lines — one-second run bucketing and reusable expression evaluation; `SeriesConfig` Qt-free primitive/base records and immutable computed-expression trees, with structured model validation of presentation, expression, identity, catalogue, and display-order invariants.

#### Settings
- Series configuration is manageable through `SettingsViewModel` via `SeriesManagementUseCase`, separate from the graph read model.
- Graph-line edits in Settings stay pending until Save or Discard — the dashboard graph updates live while editing, but nothing is written to disk until confirmed. `SeriesConfigStore` gains `beginDraft`/`commitDraft`/`discardDraft`/`hasPendingChanges`; `SettingsDialog` is now an `ApplicationWindow` so it can intercept every close path and prompt on unsaved changes.

### Changed

#### Graphing
- Main graph series are resolved through the persisted series configuration and presented with stable identities; `GraphUseCase` owns series resolution and mutations, `GraphViewModel` adapts results to Qt types.
- Graph-line enablement (persisted, in Settings) is now separate from per-line dashboard visibility (temporary, on the main graph), the latter keyed by stable series ID through a new `VisualSettingsManager`; legacy graph-line and axis preferences migrate without disabling configured series.
- Graph data loads only enabled series and no longer exposes legacy graph mutation controls — the graph read model is now read-only.
- Series configuration delegates document persistence, quarantine, and legacy disabled-column migration to the app-wide `ISettingsService`; its dedicated QSettings backend was removed.
- `SeriesModel` is promoted to a QObject exposing `id`/`name`/`color`/`column` as `Q_PROPERTY`s directly to QML via `QQmlListProperty`; the deprecated `columnName`/`columnColor`/`columnKey` base-class callbacks are removed. Scenario-history column visibility and Y-axis choice reset to defaults once, because settings now key off `SeriesModel::id` rather than the old `columnKey()` string.
- Shared-axis configuration is represented by `AxisConfig`, and series can reference a shared Y-axis.
- Corrected the seeded projected-score expressions to preserve the legacy graph columns; added the unreleased `ProjectRateToFinal` expression and its V1 JSON tag.
- Extracted the graph-series configuration row into `SeriesConfigEditorDelegate.qml`, preserving its editing and drag-reordering behavior.

#### Settings
- Series-configuration edits use one unified `updateSeries` store operation for both primitive and computed series; the split-type update APIs are gone.
- `SeriesConfig` rows no longer split into a protected "base" category and an editable "computed" one — primitive rows can be renamed, re-expressed, and deleted, and name validation runs on every row.
- `SeriesExpressionEditorModel` owns a typed `EditableExpressionNode` tree with pointer-based structural editing and targeted root/selection notifications; serialization walks the typed nodes.
- Removed a dead persistence-failure branch from `SeriesConfigStore`; Release builds now gate `qDebug()` output via `QT_NO_DEBUG_OUTPUT`.

#### Architecture
- `IScenarioBrowserUseCase::onChanged` drops its `QObject*` context parameter, matching `ISeriesManagementUseCase`; `i_scenario_browser_use_case.h` is now Qt-free and covered by the header-set guard. Plus small CMake/QML-registration consistency fixes.

#### User Interface
- Series-configuration rows use a contrasting darkened surface, a brighter 2×3-dot grip, hover highlighting, and a compact `ƒₓ` expression-editor control (with tooltip and accessible name) to clarify their editing affordances.

### Fixed

#### Scenario & run selection
- Reactivating an already-active scenario hash under a different name no longer lets the view model's cached name drift from the domain's first-name-wins identity policy.

#### Graphing
- Several graph lines (Kills, Dmg, Score Total, Expected Final Score, and the "(5s)" variant) rendered under the wrong name/color, or not at all, once `Hits` got its own config entry. Plot slots now key off `SeriesId` instead of display position; `GraphViewModel::Column` and its static presentation table are removed, and Y-axis grouping keys by `SeriesId` with a per-series fallback.
- The scenario-history graph no longer renders a Y-axis when there is no data.
- The hover info box now respects series-color alpha.
- `UserProfile::rollingTimeAverageFor()` no longer allocates a dense per-calendar-day vector across the full first-to-last-run range; output is bounded by recorded play days, with identical results for every existing case.
- `GraphUseCase::load_perf()` builds a length-bounded `std::string` from its `string_view` argument instead of reading to the next NUL byte; a stray diagnostic line was removed.

#### Settings
- `AverageAccrossRuns` no longer raises an error when instantiated.
- A series' enabled state now persists.
- The expression-editor dialog sizes itself to the expression being edited and scrolls instead of clipping when it overflows the window.
- The graph-line list in Settings scrolls instead of pushing the add/error/Save/Discard controls off-window; dragging near the edge auto-scrolls while preserving the reorder target.
- Reopening Settings after saving graph-line changes starts a fresh draft, so further edits can be saved again instead of persisting immediately with Save/Discard stuck disabled.
- Dragging to reorder a graph line no longer throws and leaves rows rendering on top of each other; drag state is reset before the model-resetting reorder call, with a regression test.
- Fixed a crash when editing deeply nested computed-series expressions; the nested-cards layout is replaced with a flat list.

#### Data & Profile
- A rejected `ScenarioPerf::add_data()` call (e.g. a float passed for `SHOTS`) no longer leaves a zero-valued phantom data point at that timestamp.
- `UserProfile::getAverageScore()` accumulates in `double`, so small run scores are no longer silently dropped once the running total crosses float32's 8-ULP range.
- Fixed a crash when KovaaKs renamed or deleted a newly discovered `.perf` file before it could be decoded; `ProfileService`/`ProfileBuilder` now skip and log the unavailable file.
- Fixed the app failing to find a KovaaKs install when no directory had ever been configured — the hardcoded default is no longer round-tripped through `QUrl`, which was misparsing the `C:` drive letter as a URL scheme.

#### Build & packaging
- Release builds no longer open a console window alongside the app — `ksv` links as a GUI-subsystem binary in Release (`WIN32_EXECUTABLE`), console-subsystem in Debug so `qDebug()`/`qWarning()` still surface during development.
- `qDebug()` output from library code (`App`, `FileService`, `SeriesConfigStore`, …) is now suppressed in Release. `QT_NO_DEBUG_OUTPUT` moved from a `ksv`-only PRIVATE definition, which the per-layer static libs did not inherit, to a directory-scoped one applied before `add_subdirectory()`.

#### User Interface
- Hiding a graph line now stays hidden after restart; the "seen series IDs" set is persisted in a `Settings` block instead of resetting to empty each launch.
- Help > About shows the app name and version instead of a placeholder.

### Removed

#### Graphing
- Deleted `GraphViewModel::fetchData()`'s legacy fallback branch and the entire `GraphSeries`/`PerfColumnBuilder`/`ColumnId` pipeline it was the last caller of; `IGraphUseCase`/`GraphUseCase` lose `get_series()`. Fixed a latent bug this exposed: `seriesPoints()` had returned an all-zero `y()` for non-time columns since the SeriesConfig migration.
- `GraphViewModelBase`'s five unused legacy read-model methods (`plottableColumns`, `axisBounds`, `axisTicks`, `seriesPoints`, `xColumn`) and their backing storage.
- `IGraphLineConfig`, `qt_data::GraphLineConfig`, the `GraphColumnPreferences` use case + `IGraphColumnPreferences`, and the empty `SeriesConfigValidator` — all superseded by `ISeriesConfigStore` with no remaining callers.

#### Data & Profile
- `IFileService::getLatestPerf()` — unused; the live path is `IProfileService::getLatestPerf()` on a different interface.

#### Architecture
- Cleaned up unmarked commented-out code across `app.h`, `file_service.h`, and `AppMenuBar.qml`; `Gallery.qml`'s parked blocks now carry a retiring `TODO`, and a stale-code marker date format was fixed.

## v0.5.1-beta

### Added

#### Scenario & run selection
- Personal-best runs are now flagged with a badge in scenario and recent-run lists.

### Changed

#### Scenario & run selection
- `ScenarioBrowserUseCase` now calculates the personal-best flag from a run's full chronological history, exposed via a new `RunListModel` role for the UI to consume.

#### Graphing
- Score Total, Expected Final Score, and Expected Final Score (5s) now share y-axis bounds, making their values directly comparable on the graph.

#### Data & Profile
- Profile loading now distinguishes an absent store from an unparseable or schema-mismatched one, so `ProfileService` can report the specific rejection reason instead of a generic fallback.

#### Architecture
- Future CHANGELOG.md sections group changes by type before area.
- `ScenarioBrowserUseCase` now owns scenario-browser aggregation, selected-run access, and refresh notifications, so `ScenarioBrowserViewModel` depends on a narrow contract instead of `ISessionController`.

### Fixed

#### Data & Profile
- A failed or interrupted save can no longer corrupt your profile — the previous store is left intact, and a failed save is now reported instead of silently ignored.

#### Graphing
- Run graphs no longer open on a spurious zero-value point at 0s, which was also pulling the y-axis down to 0 unnecessarily.
- Expected Final Score and Expected Final Score (5s) now project accurately throughout a run instead of consistently landing under the real total, especially on scenarios that manipulate time flow.

#### User Interface
- The y-axis picker in Graph Lines settings now lists one entry per axis, so columns sharing an axis (Score Total, Expected Final Score, Expected Final Score (5s)) no longer appear as separate, functionally identical choices.

## v0.5.0-beta

### Scenario & run selection

**Added**
- Scenario list can be sorted by run count, name, or last played, via a sort control beside the search field.

**Changed**
- Sort selection moved next to the search bar; the default is now "Last Played" rather than "Runs".
- Run list data is now the domain-owned `RunPerformance` value, with accuracy computed alongside completion statistics. Duration sorting is gone from run lists as a result.

**Fixed**
- The scenario panel no longer resizes when a scenario or run is clicked. Its width is derived once from the unfiltered scenario catalogue when a profile loads and clamped to a third of the window, so a model reset can no longer move the panel's size hint.
- Run labels survive an OS failure to convert a timestamp to local time, falling back to the scenario name instead of formatting from unset state.

**Removed**
- Dropped three non-functional `ControlPanel.qml` controls — a scenario filter dropdown, an empty display-mode dropdown, and a "Rendering" label that never updated. Real scenario filtering already runs through `ScenarioBrowserViewModel`.

### Graphing

**Added**
- New scenario completion history graph plotting score, accuracy, shots, hits and misses across every run of the current scenario, on an integral run-index axis with independent metric axes. `UserProfile::getCompletionHistory()` projects chronological totals without copying full run data.
- Dashboard graph lines can be enabled or disabled independently of visibility, so a disabled line leaves the control panel and View menu without losing its visibility choice. Disabled keys persist as stable column keys, and unknown keys are preserved across updates so a future column is never silently dropped.
- The labelled y-axis metric is selectable from Settings → Graph Lines and persists across launches. Stored as a stable column key rather than an index, and written only on explicit selection so temporarily hiding a column cannot clobber the preference.
- A rotated y-axis title names the metric the tick numbers belong to, sized against the plot rect so it stays centered regardless of rotation.

**Changed**
- Axis margins are measured from the actual tick label text rather than fixed constants, using the same font metrics the painter draws with so measurement cannot drift from what is rendered.
- Date-time axes have their own UTC epoch-millisecond range API in `AxisModel`, giving playtime dates calendar-aligned bounds and ticks instead of arbitrary epoch-day intervals. Numeric baseline handling stays unavailable to calendar axes by construction.

**Fixed**
- "Have Graph Load Latest Performance File" now always loads the most recent run rather than reloading the current selection.
- The graph shows the most recent run at startup instead of staying blank until first interaction.

### Settings

**Changed**
- "Generate Profile" moved into Settings under the renamed "Profile" category; the "Have Graph Load Latest Performance File" button was removed.

**Fixed**
- The Settings dialog opens centered on the window instead of pinned to the top-left corner. It is now instantiated directly rather than behind a `Loader`, whose zero-size item was what `anchors.centerIn` resolved against.

### Data & Profile

**Added**
- Profile generation reports progress as runs are scanned. `IFileService::listPerfFiles()` separates discovery from decoding so per-file completion can be reported; the progress bar is indeterminate until the total is known.

**Changed**
- Profile generation runs off the UI thread on a worker `QThread`, keeping the window responsive. All three build paths go through one request mechanism. Runs arriving mid-build are queued and replayed against the finished profile rather than applied to one about to be discarded; a build whose source directory no longer matches is dropped while its pending queue survives for the follow-up; and a generate requested during a build coalesces into a single rebuild. `SettingsService` is mutex-guarded, since the worker reaches `QSettings` while the UI thread reads and writes the same instance.
- Profiles can ingest runs from multiple configured KovaaKs directories, persisting an immutable source-directory registry. Each run stores a directory id and filename instead of a duplicated absolute path, and settings retain the legacy single-directory value while supporting a list internally.
- The profile file is treated as the authoritative store rather than a derived cache, throughout the schema, serializer, settings surfaces, tests and documentation. The default filename is now `profile.pb`.
- Profile stores carry a file wrapper whose leading header exposes format version, creation time, and store name ahead of the run body. Malformed headers and incompatible header versions are rejected before any run is loaded, and `readHeader()` allows side-effect-free metadata inspection.

**Fixed**
- Run stats (shots, hits, misses, kills, damage, score) are accumulated rather than overwritten when several events land in the same timestamp bucket, so they are no longer undercounted.
- A profile file that cannot be parsed, or whose version does not match, is quarantined under a timestamped content-derived name instead of being overwritten by the rebuild.

### Build & packaging

**Added**
- `KSV_VERSION` is passed to the `ksv` target as a compile definition and set as the application version, making it readable from QML as `Qt.application.version`. No UI consumes it yet.

**Changed**
- `ksv_gallery` is skipped whenever CMake configures a Release build, keeping the development component gallery out of shipped packages.

### Architecture

**Added**
- `ksv_contracts` makes the app-to-presentation contract explicit and Qt-free. Its CMake-generated header verifier is `EXCLUDE_FROM_ALL` and built by CTest, so the boundary is enforced rather than merely documented.
- `docs/FIXES.md`, a checkable, source-located record of outstanding static-audit findings.

**Changed**
- `ProfileBuilder` extracts the scan/decode/aggregate half of profile generation into a `ksv_data` class holding only an `IFileService`, freed of every `ProfileService` member so it can run off the UI thread. `setProfile()` is deliberately kept off `IProfileService`, and `saveProfile()` stays at the caller so the cache-hit path does not write back on every startup.
- Graph column ids have explicit values and application-owned stable string keys, letting persistent preferences and app-layer selection logic identify columns without depending on presentation metadata.
- Changelog fragment guidance distinguishes single newlines grouping related implementation points from blank lines separating independent changes; the fragment and changelog templates no longer hand-wrap to a fixed width, and the `type` and `area` enums sit on separate lines so either can be found independently.

### User Interface

**Added**
- A first-run banner prompts for the KovaaKs folder before statistics are shown, driven by a new `SettingsViewModel` flag for whether a folder has ever been configured.
- The run currently rendered in the graph is highlighted in both run lists, using the app accent for a background tint, border, and left-side marker.
- A View menu with checkable entries hiding the Scenario Graph and its individual lines, the Playtime Graph, the Control Panel, and the Selection Panel's Recent Runs and Scenario Browser sections. Backed purely by QSettings, matching the existing graph-column-visibility pattern, so choices persist between launches with no VM involvement.

**Changed**
- One consistent dark theme throughout. The app runs on Fusion but the QML was written against Material's attached properties, so hand-drawn surfaces and real controls resolved to two different palettes. Fusion is now pinned to a dark colour scheme in all three entry points, overriding the OS light/dark setting, and the accent is defined once in `Main.qml` and propagates to every control and popup. All Material imports and hardcoded surface colours are gone, replaced by palette roles; graph series colours are data colours and were left alone.
- "Load Performance File..." moved from the sidebar into File → Load Performance File..., following the menu bar's existing signal-out pattern.
- The duplicated scenario and run-list sorting controls are now one shared QML component.

**Fixed**
- File → Quit closes the app and Help → About opens a placeholder dialog; both actions were previously dead. File → New, which did nothing, is removed. Quit routes through a signal rather than calling `Qt.quit()` inline, so the menu bar stays testable under the QML test runner.

## v0.4.1-alpha

### Data & profile cache

**Fixed**
- New runs completed while the app is open are now picked up. `FileService` was forwarding the watched
  directory's own path (from `QFileSystemWatcher::directoryChanged`) to `onFilesChanged` subscribers
  instead of the newly-created file's path, so `ProfileService::addPerfFileToProfile` tried to decode
  the directory itself and failed silently. `FileService` now diffs `QDir::entryList` snapshots to
  report the actual new file(s).
- Changing the KovaaKs directory in Settings now takes effect immediately. `ISettingsService` gained
  `onKovaaksDirChanged`, mirroring `onProfilePathChanged`, and `SettingsService::setKovaaksDir` fires
  it. `FileService` subscribes and repoints its `QFileSystemWatcher` to the new
  `FPSAimTrainer/performances` directory, re-snapshotting `m_known_files` so pre-existing files there
  aren't reported as new.
- `ProfileService::addPerfFileToProfile` now guards `m_profile` for null, matching the pattern already
  used by `saveProfile()` and every read accessor. It previously dereferenced `m_profile`
  unconditionally, which could crash if `IFileService::onFilesChanged`'s callback fired (via
  `QFileSystemWatcher`) between the subscription being registered in the constructor and `loadProfile()`
  running in `App::App()`.

### Build & packaging

**Changed**
- Changelog assembly split out of the release procedure into its own `changelog-assemble` skill, so the
  assembled sections can be previewed between releases; `/release` Step 2 now delegates to it. The
  format rules moved out of the skill and `AGENTS.md` into three tracked `docs/templates/` documents
  (fragment, CHANGELOG.md, RELEASE_NOTES.md), so the conventions reach anyone cloning the repo. The
  release-notes shape now splits by change type at the top level — `### Features & changes` and a flat
  `### Bug fixes` — while CHANGELOG.md keeps area-then-bucket.

**Fixed**
- CMakeLists.txt is now the single source of the version: `KSV_VERSION_SUFFIX` carries the pre-release
  suffix `project(VERSION)` cannot hold, and the new `scripts/version.ps1` is the only parser, so
  packaging and future release automation share one reader. `package-release.ps1` fixes: `$RepoRoot`/
  `$BuildDir` are now resolved in the body (as a `-File` param default, `Resolve-Path "$PSScriptRoot/.."`
  ran before `$PSScriptRoot` was populated and built into `C:\build-release`); the build directory is
  deleted before configuring unless `-Incremental`, since a reused dir can silently package a stale app,
  with a post-configure assertion reading `CMAKE_BUILD_TYPE`/`VCPKG_TARGET_TRIPLET`/`CMAKE_CXX_COMPILER`
  back out of the cache; the zip now archives the dist folder rather than its contents, with entry names
  normalised to `/` (5.1's `Compress-Archive`/`ZipFile` write backslashes the ZIP spec forbids); a
  missing MinGW runtime DLL now throws instead of warning; a new smoke test (default on, `-SkipSmokeTest`
  opts out) launches the packaged exe with the toolchain dirs stripped from `PATH` and asserts on the
  loaded-module list rather than survival; `-ValidateOnly` runs just the tool-path assertions. `.ps1`
  files under `scripts/` must stay ASCII — 5.1 reads a BOM-less script as ANSI.

### Architecture

**Added**
- Changelog split into a technical `CHANGELOG.md` and a user-facing `RELEASE_NOTES.md`, both fed by
  per-change fragments in `changelog.d/`. Fragments carry `type`/`area`, an optional `user` line that
  promotes the change into the release notes, and a developer body; they are assembled and deleted at
  version bump. Chosen over a git-hook design so entries survive `--amend`/rebase and are committed with
  the change they describe.

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
