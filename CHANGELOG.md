# Changelog

The technical record — every change, user-visible or not. For the user-facing subset, see
[RELEASE_NOTES.md](RELEASE_NOTES.md). Pending entries live as fragments in `changelog.d/` and are
assembled into both files at version bump.

## v1.0.0-rc.1

### Added

#### Scenario & run selection
- Clicking the Scenario History graph selects the run nearest the click. The run list scrolls to that run and the run graph shows it. `ICompletionHistoryUseCase::select_run` goes through `ISessionController`, so every selection path shares one current run.

#### Graphing
- Series expressions have a compact one-line text form (DSL) that can be copied and pasted in the expression editor. The codec lives in `ksv_contracts` and has no Qt dependency. Encoding is canonical, so decoding and re-encoding gives back the same bytes. This is what lets the old JSON codecs be retired.
- The run graph explains why it is empty: "No run selected", or "Selected run does not support performance analysis" for CSV-only runs. This comes from a new `GraphViewModel::ContentState`.
- Scenario History marks the selected run and follows selection from the run list and Recent. The `selected_run_index` contract supports this.
- Per-graph configuration: each graph has a gear popup for quick settings, and there is a new Graphs settings page. Both cover the visible range (recent days, custom dates, latest N runs), the playtime averaging window, line style and markers, series colours and widths, and how each axis fits (fit visible data, start at zero, fixed bounds). How it is built:
  - `GraphConfigurationService` is the single owner of the committed configuration. It validates a whole candidate before it writes or publishes anything, and `SeriesConfigStore`/`SeriesManagementUseCase` are retired.
  - Saved as schema v4 of the series document. Each section is quarantined on its own, so one bad graph section doesn't reset the others.
  - The Settings window edits one working copy for each visit and commits it on Save.
  - Style changes take a separate path, so they never re-query the profile or re-evaluate a benchmark.
  - Range projection keeps the original run numbers and full-history values. `IDaySource` keeps "today"-anchored ranges current while the app stays open.
- Scenario History has a clickable metric legend (shared with the View menu) and a rolling average of the last 10 scores. The average is computed over the whole history before the range is applied, so the range never changes a run's value.
- Run graph lines can hide their first 0–10 points. The Expected Final Score lines hide 1 by default. The setting is in a new options menu on each Graph Lines row. The graph configuration schema is now 5.

#### Benchmarks
- Benchmark library foundation: a Qt-free benchmark model and completeness validator, and a filesystem store with `schemaVersion` gating, atomic writes guarded by a content digest, and per-file classification.
- Benchmark authoring: create a benchmark, import one from a KovaaK's playlist, then edit, validate, save, rename, delete and reopen it. Incomplete drafts are kept, and any action that would replace the draft confirms first.
- Benchmark evaluation: the pure `evaluateBenchmark` projection gives per-scenario PB, recent average, tier and next threshold, plus official attained and completed rank with next-tier blockers, average rank, and a fully rebuilt rank history. Name-based definitions are matched to hash-based runs, and unique matches are saved automatically. Benchmark playtime uses the same accounting as the profile playtime graph, so the two can't drift apart.
- New Benchmarks workspace tab. It shows rank status, next-tier blockers, rank and playtime history, and a scenario breakdown, all read from one consistent snapshot. The Benchmark Manager shows each scenario's match state, can add scenarios you have already played, resolves ambiguous names, and can change or clear a mapping without losing its thresholds.
- Drag scenarios between rows in the benchmark editor to set their order, within a group or into another one.
- Each category and subcategory shows its rank as a chip in that tier's colour.
- Each scenario's tier bar marks your recent average with a tick.
- Tier bars have distinct looks for unplayed scenarios, mappings that need fixing, scores above the top tier, and hatched tiers with no usable threshold. A threshold that is out of order hatches every chunk it bounds instead of filling from a value that may be wrong.
- The Benchmark Manager shows "• Unsaved" while the open benchmark has unsaved changes. Undoing back to the saved copy clears it.
- Each tier bar shows the personal best where its fill ends, in the colour of the rank you've reached.
- Benchmarks marked incomplete, invalid or unsupported get an edit button that opens the Benchmark Manager on that benchmark without selecting it.
- One benchmark row can link several versions (hashes) of a scenario. Their histories are merged in date order for PB, recent average and progress, and overlapping playtime is counted once. The store writes schema 2 and still reads schema 1.

#### Data & Profile
- Older KovaaK's runs that only have a CSV now appear in run history. A facet-based `Run` model replaces `ScenarioPerf`, `RunIngestor` handles file pairing for every ingest path, and the profile store moves to `profile.proto` v4 with the v3 upgrade kept in a migrator that can be dropped later.
- Incremental profile import: the profile catches up on runs played while the app was closed, and Refresh Profile (File menu, F5, and Settings) imports new runs on demand. One `ProfileImporter` serves startup catch-up, manual refresh and live file notifications. It skips sources it has already recorded, and a late perf or CSV sibling enriches the existing run instead of adding a new one. Each changed batch is saved once and published only after the save succeeds; an unchanged refresh neither saves nor publishes.

#### Build & packaging
- `scripts/build_and_test.py` is the single entry point for Debug builds and tests. It has test scopes and wildcard matching, prints one line on success, and keeps full logs. Toolchain paths come from layered defaults and local overrides.
- `ksv_driver` and `scripts/ui_driver.py`: a development-only UI driver that runs the real `App` against a throwaway session and can inspect, click, type and take screenshots without a display.
- `scripts/make_dummy_kovaaks.py` generates a fake KovaaK's directory for testing the app on a machine without KovaaK's.
- UI driver profile specs (`--profile-spec`) build test data from a declarative JSON file. Fault injection (`--inject`) can delay or fail decoder, CSV, benchmark-store and series-store calls.
- UI driver tours and HTML contact-sheet reports. Named shots, batch variables and sections, and checked-in `smoke`, `full` and `unreachable` tours.
- UI driver snapshot fixtures replay a real profile, benchmarks and settings. Includes `take_snapshot.py`.

#### Architecture
- `qDebug`/`qWarning`/`qCritical` output now includes file, line and function through a shared message pattern installed at startup.

#### User Interface
- Keyboard shortcuts: Ctrl+, opens Settings, Ctrl+O loads a performance file, and Ctrl+F jumps to the scenario search from either tab.

#### Documentation
- Conventions for Architecture Decision Records and the Architecture Description, plus `scripts/arch_doc.py` to validate and manage them.
- An as-built Architecture Description with runtime views for the profile lifecycle and for run selection and rendering.
- Requirements and design for the benchmark table editor.

### Changed

#### Graphing
- A series can exist with no expression; it plots nothing. "Add series" now creates a blank series. Nested expressions with a missing input are still rejected.
- `GraphViewModel` reads series points and the run duration through separate `IGraphUseCase` accessors. Refreshing metadata no longer evaluates series values.
- Series configuration files store expressions as readable DSL text (schema v3). Older v1 and v2 JSON expressions still load.
- Gridlines use a stronger colour.
- On hover, graphs show a coloured marker on each line instead of a dot on every data point.
- `AxisModel` is replaced by a read-only `Axis` contract with persistent `ValueAxis` and `DateTimeAxis` classes. Benchmark history graphs share one date axis.
- Scenarios tab graph panels:
  - Line toggles become a scrolling legend down the left side of the run graph.
  - Lines are stacked in Graph Lines order, with the top entry drawn on top.
  - The three panels share the height equally.
  - The coloured Y axis and its caption are removed.
- Scenario History now sits directly under the run graph, with Daily Playtime last. The View menu uses the same order.
- `ValueAxis::setRange` is no longer `[[nodiscard]]`, which removes repeated build warnings.
- The benchmark "Personal-best average rank" line runs flat to the graph's right edge.
- The benchmark rank graph draws a reference line in each tier's colour at every tier level. The rank line is now a neutral colour so it can't be mistaken for a tier. Graphs can now use a neutral (foreground) series colour.
- Graph configuration cleanup:
  - Rows on the Graphs page are keyed, so typing in a field no longer loses focus after one character.
  - Line-style controls and the catalogue are shared between the gear popup and the Graphs page.
  - Contract helpers are consolidated (`SeriesStyle`, `connectionModeName`, `applyAxisPolicy`).
- The graph settings gear is visible on the dark theme and stays highlighted while its popup is open. The popup has a proper frame. A new shared `ChromeIconButton` holds the styling.
- New default graph settings:
  - The run graph's default lineup is score, projected final score against top and recent runs, and running accuracy.
  - The other graphs open on a recent range, and the playtime window defaults to 7 days.
  - Existing settings are kept. "Restore default lines" switches to the new lineup.
- New default run graph colours: green score, cyan accuracy, and magenta, rose and violet forecasts.
- Scenario History marks the selected run with a slim translucent cyan band drawn behind the series, replacing the bright line and rings.
- The Y-axis title shows the axis name instead of listing every line on that axis. `GraphCanvas::labelledYAxisTitle` replaces the name-joining code in QML.

#### Benchmarks
- Benchmark ownership is split along layer boundaries (ADR 0005):
  - The accepted library is a Qt-free `BenchmarksService` over `IBenchmarkStore`.
  - Matching profile runs to benchmarks moves into `BenchmarkResolutionUseCase` with an edit lease.
  - Editing is a pure `domain::BenchmarkEditor` driven by `BenchmarkManagerViewModel`.
- Benchmarks with an unnamed tier, scenario, category or subcategory are flagged as incomplete, and threshold problems point at the exact tier cell.
- The benchmark editor becomes a compact scenario × rank table. It supports spreadsheet paste, fixing invalid thresholds in place, and one Undo history for the session.
- Faster table editing: multi-cell selection, drag-and-drop grouping, add buttons in the table, and context menus.
- Benchmark Manager layout:
  - Double-click or Enter opens a benchmark.
  - The library list sits in a resizable pane that can be hidden.
  - The add controls are labelled and sit right under the last row.
  - A `TableView` bug that left the rightmost ranks unreachable is fixed.
- Benchmarks tab clarity:
  - It opens on the last-viewed benchmark, or the only one.
  - Next-tier progress is shown as bars, and rank names appear in their tier colours.
  - Empty groups and the "Resolved" label are hidden.
  - An empty library shows a single call to action.
- The average rank is shown as a tier bar, with a tick where the completed rank ends.
- The scenario mapping row in the Benchmark Manager is always shown, so selecting a cell no longer pushes the table down.
- A single scenario progress grid, laid out like the editor table, replaces the blocker list and the breakdown table. Each scenario gets a tier-coloured bar up to the top tier, and blockers are marked.
- Rank columns in the editor share the free width, so a five-rank benchmark fits at the default size. They never cut off a rank name.
- A PB above the top tier is marked with a chevron past the end of the bar instead of a diamond.
- The progress grid's PB and Recent columns are replaced by PB date and play count. The PB date is the earliest run that reached the PB score.
- A category with no subcategories spans the Category and Subcategory columns, in both the editor table and the progress grid.
- The progress heading counts the scenarios that block the next tier, matching the marked rows.
- Tier bar threshold labels:
  - Each bar labels every tier with the score needed to reach it.
  - A label moves to the middle or start of its chunk instead of disappearing under the PB.
  - Labels also avoid the recent-average marker, and the marker breaks behind the PB.
  - A label inside a partly filled chunk is outlined, to show it is still a target.
- A progress grid too wide for the window opens scrolled to its right end. The Category, Subcategory and Scenario columns stay pinned while the rest scrolls, and a user's scroll position is kept until the benchmark changes.
- The benchmark selector shows only names for normal benchmarks. Incomplete, invalid and unsupported entries get coloured badges.
- Each section of a tier bar again shows progress toward the tier it is named after, labelled at its end with the score that completes it. The average-rank bar reads the same way, on the same scale as the domain's normalized rank.
- The progress grid fits more tiers before it scrolls: PB date and Plays come before the bar, tier headers are coloured names without swatches, and chunks are only as wide as the longest tier name.
- The average rank reads as a tier and progress ("Bronze · 60% to Silver") in the rank graph tooltip and the summary bar.
- An incomplete benchmark's page shows the progress table before the playtime chart.
- The average rank graph caps its tiers at one above your best rank, so the line isn't squashed by tiers you haven't reached.
- The benchmark name and summary stay pinned while you scroll:
  - A searchable dropdown replaces the benchmark list, ordered by most recently viewed. A new `IRecentBenchmarks` holds that list and replaces `lastSelectedId`.
  - Manage Benchmarks moves into the dropdown.
  - Next tier shows how many scenarios block it.
- Test coverage:
  - The hidden progress heading for non-trackable benchmarks is now actually tested.
  - The selector's "unsupported" label is now tested.
  - An audit of the benchmark acceptance criteria added tests for every gap it found.

#### Settings
- Settings opens larger (860×620) and reopens at the size you last left it.
- During a profile rebuild, the Profile page says "Rebuilding" and shows files done out of the total.
- Settings are stored in `%APPDATA%\Lecka\KovaaksStatsViewer.ini` instead of the registry, including the QML `Settings {}` blocks. Registry settings are deliberately not migrated. The gallery now redirects its settings so it can't overwrite the real file.

#### Data & Profile
- Files that fail to import are remembered in the profile (`Store.failed_sources`, an additive field, so there is no store version bump). Automatic imports (startup, live, source-settings changes) skip a recorded failure while the file is unchanged. Refresh Profile, Rebuild and the backoff retry still re-read it, and any change to the file clears the skip. A failures-only result is saved without reloading the graphs.
- Profile import status moves into a slim bar at the bottom of the window (`ProfileStatusBar.qml`, replacing `ProfileImportStatus.qml`). It appears only while busy, after a completed import worth noting, or for new problems. Settings → Profile shows Refresh and Rebuild side by side, lists problem files by name, and asks before rebuilding.

#### Build & packaging
- `build_and_test.py` takes an exclusive lock on `build-agent/`, so a second concurrent run fails fast instead of corrupting the build tree.
- Build and test failures are read from the frameworks' own reports (GoogleTest XML, Qt Test text, CTest JUnit) and shown as one compact keyed report that drops nothing.
- Build job count and process priority are machine-local settings (`KSV_BUILD_JOBS`, `KSV_BUILD_PRIORITY`). Linked git worktrees fall back to the main checkout's `toolchain.local.env`.
- UI driver input and selectors:
  - Mouse press, move and release can be held separately, and hover moves realistically.
  - New `obj:` and `rawtext:` selectors.
  - Selector text ignores menu mnemonics.
  - Dialog and popup windows get stable owner-based names.
- UI driver datasets ship fixed benchmark fixtures (`driver-demo`, and the 12-tier `driver-many-tiers`) in place of a generated benchmark where every row had the same state.
- Test build and run time cut: one CTest test per GoogleTest executable, `ctest -j4`, and precompiled headers on the heavier test targets. A clean build plus full test run went from 434 s to 273 s.

#### Architecture
- Expression transport between C++ and QML uses canonical DSL text instead of a QVariantMap codec.
- Shared test fixtures and support targets replace duplicated test scaffolding. Integration suites build the wired `App` once per suite, which roughly halves their run time.
- Benchmark library snapshot DTOs move into `ksv_contracts`, so UI code can't reach the data-layer repository port through the presentation contract.
- `App`'s injecting constructor requires `IStatsCsvParser` and `IBenchmarkStore` with no null fallback. Before this, test fixtures silently shared one AppData benchmarks directory.
- Test suite review: 118 tests that earned nothing are removed. About 146 tests no longer depend on shipped defaults, and invariant tests replace the default-table tests.

#### User Interface
- Copy, paste, delete and the expression editor (ƒx) buttons show icons instead of text.
- The component gallery serves deterministic synthetic profile datasets and the real production graph panels and expression editor.
- Runs in the Scenarios tab are two-line cards again, with a clear hover highlight and a hand cursor. This restores them after the one-line table rows.
- Scenarios tab left column:
  - The Recent section has a fixed size with a visible scroll bar.
  - The sort direction button says whether it is ascending or descending.
  - An empty scenario list explains how runs get there.
  - The first launch shows a welcome screen for choosing the Kovaaks folder.
- The Scenarios and Benchmarks workspace tabs are larger and clearly mark the active tab.
- Chart titles share one style (`ChartTitle.qml`): left-aligned, semibold, 14px.
- "Set Source Directory..." and "Settings..." end in an ellipsis, like the other menu items that open a dialog.
- The menu bar reads File, View, Benchmarks, Help.
- QML sources are regrouped by feature. Group-structure rules move into C++, and shared components (`Theme`, `CreationField`, `ToolbarButton`, grid cells and others) replace inline copies. The benchmark dialog, table and grid are split into smaller components.

### Removed

#### Benchmarks
- The Benchmark Manager's one-click Discard (X) button is removed. Leaving unsaved changes always goes through the Save/Discard prompt.
- Editing and saving existing benchmarks no longer asks for confirmation about recalculating history.

### Fixed

#### Scenario & run selection
- Selecting a scenario now switches both graphs to its latest run straight away. Before, they stayed on the previous scenario until you clicked one of its runs. The scenario browser also follows the current run when it changes elsewhere.

#### Graphing
- The completion history score axis no longer always starts at 0.
- Graph readability:
  - Axis labels no longer repeat or get cut off at the edges.
  - Line toggles sit inside the run graph panel.
  - Run history is dotted when there are few runs and no longer labels a run "#0".
  - The playtime graph shows a message instead of empty axes.
  - The run graph no longer shows a "1970-01-01" title when no run is selected.
- Daily and benchmark playtime graphs show zero on inactive days. Before, the line joined activity across the gaps.
- Date and time axes fit the data range exactly, with no extra space before or after it.

#### Benchmarks
- The benchmark rank and playtime history graphs draw their lines. Before, they showed only empty axes.
- The editor's Undo, Paste and Save buttons are compact icon buttons on one row. Before, they wrapped onto two rows at the default size.
- Naming a new subcategory and pressing Enter no longer opens a rename box on the category behind the relocation prompt. Focus now lands on the new subcategory, or on the category if you cancel.
- Escape with the scenario suggestion list open only closes the list. Before, it committed the typed name as a rename.
- Hiding and showing the library list no longer leaves the rank columns scrolled sideways with an empty gap.
- Squeezing the editor pane keeps at least one rank column and its scrollbar in view, with the identifying columns narrowing to their content. Before, the ranks disappeared entirely.
- Opening a benchmark with wider rank names no longer leaves the ranks scrolled part-way into the first rank.
- Scenarios pasted or typed into a benchmark link to your runs as soon as you save. Before, they showed "Linking to your runs…" until the app restarted. Names that differ only by capitalisation or extra whitespace now match.
- With "End date ranges at latest recorded run" on, the rank history graph ends at the latest rank improvement for every range instead of stretching past it. The rank line is now drawn over the tier lines.
- `tst_BenchmarkPicker.qml` no longer fails intermittently: it waits for the popup layout to settle before clicking.

#### Settings
- Long paths in Settings > Profile wrap, so the whole path stays readable. The rebuild button is labelled "Rebuild Profile from Kovaaks Directory".

#### Data & Profile
- Paused CSV-only runs report the real scenario length instead of counting paused time.
- Corrupt performance files and mismatched stats files no longer create or overwrite run data.

#### Build & packaging
- The About dialog in `ksv_driver` and `ksv_gallery` shows the app version.
- UI driver sessions can no longer import the developer's real KovaaK's install. Sessions can be composed from overlays, settings and environment variables, and new file commands (`put`, `rm`, `edit`) wait until the app is idle.

#### User Interface
- View menu labels line up. Before, submenus and plain items sat further left than the items with check boxes.

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
