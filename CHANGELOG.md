# Changelog

The technical record — every change, user-visible or not. For the user-facing subset, see
[RELEASE_NOTES.md](RELEASE_NOTES.md). Pending entries live as fragments in `changelog.d/` and are
assembled into both files at version bump.

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
