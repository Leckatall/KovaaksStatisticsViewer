# Changelog

The technical record — every change, user-visible or not. For the user-facing subset, see
[RELEASE_NOTES.md](RELEASE_NOTES.md). Pending entries live as fragments in `changelog.d/` and are
assembled into both files at version bump.

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
