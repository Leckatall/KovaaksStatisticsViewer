---
status: approved
---
# Graph configuration design

## Purpose and design scope

This proposal designs the configuration capability in the [graph configuration requirements](requirements.md): independent preferences for five graph types, immediate quick adjustments, one isolated detailed-settings draft including Graph Lines, history-range projections, and shared rendering controls.

It preserves profile authority, benchmark evaluation and completeness, scenario/run identity, existing computed-series capabilities, and the established layer boundaries. It does not specify an implementation task list, introduce new measurements or spline algorithms, or maintain the Architecture Description or ADRs.

The developer confirmed the design for synthesis. This document remains proposed until the resulting document is explicitly accepted.

## Design basis

The current implementation establishes these relevant facts:

- [`SeriesConfigStore`](../../../src/qt_data/series_config_store.cpp) owns accepted series/axis data, persistence, and a draft shadow in the same object. Draft mutations change the data returned by `getAll()` and emit configuration callbacks before commit. [`GraphUseCase`](../../../src/app/usecases/graph_use_case.h) consumes that same store, so its current draft is not isolated from the dashboard.
- Series configuration is schema-version 3 JSON under QSettings key `graph/seriesConfigV1`. The codec supports versions 1 and 2, stable decimal-string IDs, expressions, axes, and exhausted ID allocators. [`SettingsService`](../../../src/qt_data/settings_service.cpp) submits the document and calls `sync()` without checking write status.
- [`SettingsViewModel`](../../../src/ui/presentation/app/settings_vm.h) delegates series editing and draft lifecycle through `ISeriesManagementUseCase`. [`SettingsDialog`](../../../src/ui/qml/shell/SettingsDialog.qml) starts that draft on open and has a Save/Discard/Cancel close prompt. Its Graph Lines axis-label selection currently writes directly to QML settings rather than that draft.
- [`PlaytimeGraphViewModel`](../../../src/ui/presentation/graphs/playtime_graph_vm.cpp) requests a constant three-day window and constructs a zero-based minutes axis. [`UserProfile::rollingTimeAverageFor()`](../../../src/domain/user_profile.cpp) owns the accounting: run-start UTC days, duration totals, an initial denominator shorter than the window, and omission of dates with a zero window sum. These semantics must survive filtering and window changes.
- [`CompletionHistoryUseCase`](../../../src/app/usecases/completion_history_use_case.h) returns full chronological scenario history with original one-based run numbers and timestamps. [`CompletionHistoryViewModel`](../../../src/ui/presentation/graphs/completion_history_vm.cpp) has score, accuracy, and shared shots/hits/misses axes. The QML panel currently refuses to show a one-run chart.
- [`BenchmarkTrackingUseCase`](../../../src/app/usecases/benchmark_tracking_use_case.cpp) caches full projections for selected benchmarks. The [evaluator](../../../src/domain/benchmarks/benchmark_evaluator.cpp) emits average-rank history on improvement days. [`BenchmarkTrackingViewModel`](../../../src/ui/presentation/benchmarks/tracking/benchmark_tracking_vm.cpp) currently shares one date axis between its charts and appends a synthetic trailing rank point. [`BenchmarkHistoryViewModel`](../../../src/ui/presentation/benchmarks/tracking/benchmark_history_vm.cpp) keeps tier labels and reference colours distinct from the neutral rank series.
- [`SeriesPainter`](../../../src/ui/components/series_painter.cpp) draws the existing monotone cubic spline and already handles one point as a marker. [`GraphCanvas`](../../../src/ui/components/graph_canvas.cpp) handles actual-sample tooltips, shared axes, reference lines, and spacing-based marker suppression. It currently does not pass a series-specific line width to the painter. [`VisualSettingsManager`](../../../src/ui/qml/graph/VisualSettingsManager.qml) separately persists visibility and axis-label selections.

The [Architecture Description](../../architecture/README.md) supplied responsibility and persistence context, but some inventories predate the benchmark implementation. Current source establishes the feature baseline. [ADR 0001](../../architecture/decisions/0001-application-interface-and-implementation-placement.md) governs application contract placement; [ADR 0004](../../architecture/decisions/0004-route-view-model-interactions-through-use-cases.md) governs view-model dependencies. [ADR 0005](../../architecture/decisions/0005-data-owned-benchmark-service-and-presentation-drafts.md) governs benchmarks specifically and supplies an existing accepted-state/presentation-draft precedent, rather than an automatic mandate for all configuration.

Discovery used Tier 2 graph verification, project `C-Users-Lecka-CLionProjects-KovaaksStatisticsViewer`, generation `2026-10-16T00:20:11Z`. Relevant graph queries were followed by targeted source reads. Coverage reported changed filesystem metadata and partial parsing in settings, series, and other QObject declarations; the relevant declarations and implementations were read directly. Tracing was not reliable enough to establish virtual-interface callback chains, so those claims rely on source. No representative-profile latency measurements or implemented prototype are available.

## Technical drivers

- A settings visit must isolate all graph edits, including computed series and axes, while popup changes immediately become effective.
- Range selection must not change the inputs to historical averaging or personal-best evaluation.
- Graph type is the preference identity; scenario and benchmark selection provide data context only.
- Original observations, carried historical state, and renderer-generated geometry must remain distinguishable.
- Existing configuration and defaults must survive the addition of preferences without rebuilding the profile.
- Numeric axes use displayed units; the rank graph retains its specialised tier meaning.
- Configuration remains reachable through empty and unavailable graph states.

## Proposed design and delta

Introduce one typed graph-configuration snapshot containing preferences for the five graph types and the existing run-series/axis document. A Qt-free data service owns the committed snapshot. A Qt adapter owns versioned JSON and QSettings access. Application use cases expose configuration operations and derive visible history projections. SettingsViewModel owns the editable working copy and input diagnostics; graph consumers only read committed state.

This separates the responsibilities currently combined in SeriesConfigStore. Its decoding, migration, validation, and series-editing behavior are reused or extracted, but its store-owned draft is retired for this flow. Compatibility facades may preserve existing consumers during realization; they must share the single accepted authority rather than retain a second mutable store.

The user-facing structure follows the [mockup](graph-popup-and-settings.excalidraw) and applicability table. QML supplies a reusable quick popup and one Graphs page. Existing graph view models remain the rendering adapters. The new configuration behavior is available through application contracts, not new direct service dependencies in view models.

## Design elements and responsibilities

Names below describe roles and proposed contracts; exact class names are implementation latitude.


| Element                                  | Owns                                                                                                                                | Boundary                                                                                |
| ---------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| `GraphConfiguration` values              | Graph keys, ranges, windows, connection modes, marker preferences, axis policies, series overrides, existing run-series/axis values | Qt-free application contracts; no files, QObjects, or profile references                |
| `GraphConfigurationService`              | Committed snapshot, in-process revision, validated whole-snapshot replacement and coherent change notification                      | Data layer over an injected persistence port; no editor working copy or profile join    |
| Configuration persistence adapter        | JSON codec, supported schema migration, legacy preference import, quarantine, QSettings submission                                  | Qt data layer; no accepted-state or draft authority                                     |
| `IGraphConfigurationUseCase`             | Effective snapshot query, editor seed, complete candidate validation/save, immediate valid quick patch                              | Application contract; coordinates service capabilities without retaining a working copy |
| History use cases and pure range helpers | Full-history calculations, relevant-run metadata, visible subsets and rank carry context                                            | Application layer over profile and benchmark contracts; no profile mutation             |
| `SettingsViewModel`                      | One working copy and baseline, raw input, validation issues, selected graph and dirty state                                         | Presentation; application actions through use cases                                     |
| Graph view models                        | Axis/series objects, labels, plot state, original-observation identity and presentation geometry                                    | Adapt application projections and committed preferences                                 |
| Popup, Graphs page, Graph Lines UI       | Layout, focus, controls, navigation, colour dialog and error presentation                                                           | Bind to presentation state; no second authoritative configuration                       |
| GraphCanvas and SeriesPainter            | Coordinate mapping, clipping, connection geometry, marker and width painting, hit testing                                           | No preference persistence, history accounting, or run selection policy                  |

Existing Graph Lines editor operations become pure transformations of the caller-owned working configuration. The view model can compose presentation-only helpers for those transformations, raw-input handling and expression editing. They do not create another settings session or write through the application boundary for each edit. Final validation is repeated at the application save boundary.

`App::App()` constructs and injects the persistence adapter, accepted service and use cases once. New persistence or clock leaves remain explicit in the injected composition used by integration tests and the gallery. Initial properties continue supplying the existing SettingsViewModel; no additional top-level VM is required. Any new QML-exposed C++ type still follows explicit metatype registration.

## Interfaces and interactions

### Configuration operations

The application contract supplies these capability-level operations:

- Read a coherent committed snapshot and revision, and subscribe to changed graph keys or configuration sections.
- Seed an editor from that snapshot. Reopening navigation within an already open settings visit does not reseed it.
- Validate and save a complete supplied candidate. The result distinguishes validation rejection from acceptance; acceptance means the QSettings write was submitted, not independently verified.
- Apply a quick patch addressed by graph type. Validate the candidate graph before replacing the committed snapshot. Invalid input changes nothing.

Callbacks are published after replacing the complete accepted snapshot, with no observer seeing a partially updated set of graphs. Service callbacks do not expose an editor's mutable storage. Graph refreshes consume the committed revision; draft edits emit editor notifications only.

### Quick changes and contextual navigation

1. The gear popup reads the graph's effective configuration. Unavailable content does not prevent opening it.
2. A valid control change submits a typed quick patch. On acceptance, QSettings receives the document and observers refresh the affected graph. Invalid text stays local with its issue and leaves the previous effective value intact.
3. Dismissal or Escape closes the popup and keeps accepted changes.
4. More graph settings closes the popup, opens the window-modal settings visit and selects that graph. The seed includes all preceding quick changes.

The existing window-modal settings interaction prevents normal dashboard popup editing during a detailed visit. This design does not introduce concurrent settings editors, background preference writers, or an external-edit conflict protocol. Profile updates and calendar changes can refresh graphs without changing configuration or the draft baseline.

### Detailed visit and Graph Lines

Opening a new visit copies the committed configuration into one working copy and baseline. Switching graph or category, following the DashboardGraph Graph Lines link, and returning to Graphs preserve both typed and raw input. Existing expression-editor subdialogs submit their accepted local result into that same working copy.

Save validates the complete draft, including fields belonging to other graphs. Invalid fields block the whole operation and identify the graph, group and control to revisit. A valid candidate is submitted once, becomes the committed snapshot and replaces the editor baseline; the visit remains open and clean. Cancel discards edits since the initial seed or last successful Save and closes. Reset this graph stages only that graph's presentation defaults and is itself cancellable.

Closing dirty settings offers Save, Discard, and Cancel closing. Save closes only after validation accepts the complete candidate; rejection retains the window and working copy. Discard closes without applying it. Cancel closing preserves the visit. A clean close releases the working copy. Opening or focusing an already open dialog never starts a second draft.

Graph Lines' Y-axis-label selector joins the working copy rather than writing its QML setting immediately. Its series, expression, axis creation/assignment and ordering capabilities remain intact. Existing dashboard series-visibility controls remain at their current locations and retain their existing scope; this feature does not create a duplicate visibility editor. Profile-path and source-directory operations retain their existing immediate workflows and are not made cancellable graph preferences.

### History refresh and calendar changes

History use cases distinguish full measurements from a visible projection. A profile revision, selected scenario or selected benchmark updates the relevant full-data source. Changing an averaging window recalculates that graph's playtime measurements. Range changes project from those measurements; style-only changes do not recalculate them.

An injectable day source supplies the current UTC epoch day, matching `ScenarioRunId::startDay()`. A Qt event-loop adapter checks day changes while the application is open and after resume/reactivation. A day change re-resolves today-anchored presets; it does not modify saved relative preferences or the editor's raw input. Tests can advance the supplied day without waiting for midnight.

## Data, state, and persistence

### Configuration model

Use stable graph keys such as `run`, `dailyPlaytime`, `scenarioHistory`, `benchmarkRank`, and `benchmarkPlaytime`. They never contain a selected run, scenario hash, benchmark ID or visible series position.

Each graph's applicable values include:

- A discriminated range: all history, recent calendar days, explicit inclusive dates, or latest run count. Latest count exists only for scenario history; the run graph has no range field.
- An independent latest-relevant-run anchor flag for date-capable graphs. It is retained even while all history or custom dates are selected.
- A positive whole-day averaging window for the two playtime graphs.
- A whole-graph connection mode and a separately stored line-mode marker preference. Points only forces markers without overwriting that preference.
- Stable local series-key colour and width overrides for eligible graphs. The rank series can retain a theme-neutral default separately from an explicit colour override.
- Numeric Y-axis policy per eligible axis: fit visible data, start at zero, or exact fixed bounds in displayed units. Scenario history uses stable score, accuracy and counts axis keys; shots/hits/misses share the counts policy.

The snapshot also includes existing Graph Lines series, expressions, axes, next-ID/exhaustion state and run-graph axis-label selection. IDs and expressions retain their existing representations and validation. Local presentation overrides on other graphs cannot reference or create computed-series expressions.

Raw invalid input is presentation state, not persisted configuration. Dirty comparison includes invalid raw edits so closing cannot silently lose them. Semantic validation returns locations keyed by graph, axis or series and field; QML derives locale-aware messages and focuses the affected control. Numeric parsing consumes the complete locale-formatted input rather than permissive prefix parsing. Stored dates are calendar values, never local-midnight timestamps with changing timezone offsets.

### Persistence and upgrade

Extend the existing QSettings JSON document to schema version 4, retaining `graph/seriesConfigV1` as its historical storage key. The added graph-preference and axis-label sections live alongside the existing series, axes and allocators. There is one writer for that document. QML must stop independently persisting the migrated run-axis selector after import; unrelated window, panel and visibility preferences remain in their existing locations.

Versions 1 and 2 follow the existing migrations to the current series model; version 3 supplies its existing series/axes unchanged. Add defaults for absent graph sections and import the existing QML run-axis label selection when no new value exists. Imported state is written in the new form on the normal configuration write path. Subsequent loads prefer the new document over legacy selector values. No profile format, benchmark definition or computed-series expression format changes.

Default resolution preserves each graph's current appearance and data extent: full applicable history, three-day playtime averaging, existing colours and widths, scenario-history axis assignments, zero-based playtime/count axes, and the tier-based rank axis. The new date anchor defaults off. Both benchmark charts independently start with the existing all-history union extent, so separating their axes alone does not change the default layout.

Existing series-document quarantine and recovery remain available. Decode a valid legacy series section independently from added preferences wherever the supported schema permits it; an invalid new graph section must not discard otherwise valid computed-series definitions or user axes. Preserve the invalid raw document using the existing quarantine mechanism and default the affected graph preferences. Unsupported or wholly undecodable documents follow the established document recovery path; this design does not promise recovery of an unreadable definition from graph settings.

After complete validation, submit the complete document through QSettings and publish its accepted in-memory replacement. Do not check `status()`, read values back to establish durability, add a disk transaction journal, or simulate write failures as a feature requirement. Acceptance and a clean draft do not guarantee that a failed underlying disk/registry operation survives restart. This is the developer's explicit revision to EDGE-05 and AC10; validation-before-application still prevents partial acceptance of an invalid draft.

### Range projection and observation identity

Resolve a recent N-day range as `[end - (N - 1), end]` in the existing UTC calendar-day model. Custom dates include both endpoints. Presets retain their relative form in storage. Resolve the latest-run anchor from full relevant run metadata, before filtering:


| Graph                 | Relevant source                                                                                                |
| --------------------- | -------------------------------------------------------------------------------------------------------------- |
| Daily playtime        | Full applicable profile                                                                                        |
| Scenario history      | Current scenario's chronological runs                                                                          |
| Both benchmark graphs | Current benchmark's contributing run facts under existing resolution rules, including later non-improving runs |

Do not infer the anchor from emitted average-rank improvements or nonzero rolling-playtime samples. An absent anchor preserves the preference and yields the appropriate no-history/unavailable state rather than borrowing unrelated records.

Daily-playtime series are computed using the selected window over full relevant history, then filtered. Preserve the existing shorter initial denominator and zero-window omission rules. A one-day window is existing daily accounting without additional smoothing. Do not extend accounting into invented post-history activity to fill a today-anchored range. The requested horizontal date interval can include blank days without creating samples.

Scenario latest-N selects a suffix of the full chronological rows, retaining original run numbers and IDs. Date filtering uses row timestamps, not displayed run positions. The graph's X axis spans original visible run numbers; full-history run count and visible count are distinct. Tooltips and selection address original rows, and a selected run outside the visible set has no drawn selection marker. The full selected run remains selected elsewhere. A one-row projection loads the chart and exposes its marker instead of the current two-run warning.

For benchmark rank, calculate the retrospective full history before projection. Supply the last prior improvement as context, plus actual improvements in the selected interval. In line modes, carry the established level to the range boundary and, after the last improvement, to its right edge. Do not create rank before the first known observation. Smooth and straight modes keep their respective connection treatment between actual improvements; carried portions are flat. Points only shows actual in-range improvements and no carried segment.

Separate observation storage used by tooltips, hit testing and markers from non-selectable boundary geometry. Remove the current practice of appending a carried endpoint into the observation list. A range with carried rank but no improvement can therefore draw a line without reporting an invented improvement at its boundary. With no in-range observations, its tooltip cannot snap to an out-of-range point. In Points only, explain that the range contains no improvement points while retaining configuration access. Tier references retain their full-history interpretation and are not fitted to only the filtered improvement points.

The two benchmark history view models own independent date-axis instances. For explicit or moving date ranges, each uses its resolved interval. Their all-history default can still use the existing union extent as read-only source metadata; editing one graph never writes the other graph's range. Benchmark summaries and attainment always consume the unfiltered projection.

## Failure and quality behavior

### Validation, reset, and empty content

Reject invalid/reversed dates, fractional or non-positive counts/windows, non-finite numbers, invalid widths and reversed/equal fixed bounds. Representable limits must be stated by the input control; no hidden truncation, unrelated clamping or substitution with zero is allowed. Validation of the full candidate precedes QSettings submission and accepted-state replacement.

Reset restores the selected graph's range, applicable window, appearance and numeric fitting defaults. For the run graph it resets whole-graph presentation only. It never deletes or resets Graph Lines definitions, user axes, expressions or their assignments, and it does not touch another graph. Existing visibility controls are outside this reset.

Graph view models expose distinct available-data, visible-observation, carried-state and unavailable-content facts rather than overloading one `hasData` flag. QML can distinguish no history, an empty selected range, no improvement points, no selected run, missing performance data and existing benchmark incompleteness/unavailability. Gear controls live outside loaders that depend on plottable data.

### Rendering and axis policies

Pass each committed series colour and width to the shared painter. Smooth uses the existing monotone cubic interpolation; straight uses actual consecutive observations; Points only draws markers and no data connection. Single actual observations always draw a marker. Points only must bypass density suppression that would otherwise erase its entire data representation. Preserve existing marker defaults where no preference has been saved; explicit marker changes must be reflected consistently across all visible series without overwriting the stored line-mode preference.

Render tier reference lines through the existing separate path with their original colours and labels. The rank data series keeps an explicit identifying name even when recoloured. Painter-generated spline vertices and carried boundary vertices never enter sample lookup or run selection. Clip data geometry to the configured plot area, including when fixed bounds exclude some values.

Compute automatic numeric Y ranges from the applicable visible series sharing an axis, using existing visibility choices. Use ValueTransform's displayed units, so fixed accuracy bounds are percentages and playtime bounds are minutes. Fixed bounds remain exact rather than being widened to nice numbers; tick generation can still use readable interior ticks. Empty and singleton automatic ranges use the existing finite fallback span. Run-graph fitting controls remain absent; its existing axis assignments and label selection remain in Graph Lines. The rank graph never receives generic numeric fitting.

### Responsiveness and accessibility

Retain synchronous calculation initially, with separate invalidation for full data, window, range and style. Reuse the benchmark evaluator's full-history result for range/style changes; changing benchmark playtime averaging must not redefine rank evaluation or summary totals. Cache derived series by relevant data revision, data context and averaging window. A date rollover only changes the visible projection. Bound temporary allocations by actual input/output size rather than multiplying run-day count by an arbitrarily large user window.

This is a bounded performance assumption, not measured proof of QUAL-04. Verify representative supported profiles during implementation. A delayed calculation retains the last coherent graph, the current run selection and the editor state; it never replaces the editor with an empty loading view. If measurements show synchronous work cannot meet usability, return to the design decision before introducing workers or changing publication order.

Use accessible graph-specific gear names, tooltip text, explicit control labels, units, focus indicators and error associations. Popups support keyboard traversal and Escape, and closing returns focus to the opener. The contextual settings link transfers focus into the selected graph editor. The Graphs page scrolls or reflows while Save/Cancel remain reachable at the established settings size floor and larger text scales. Single-series appearance groups name the series without a redundant selector. Colour is supplemented by names and tier labels.

## Requirements traceability and verification

These are verification seams and representative invariants, not an exhaustive test catalogue or evidence of implementation. Use GoogleTest and Qt Quick Test; do not verify behavior by driving the running app. Existing [store tests](../../../tests/qt_data/series_config_store_test.cpp), [settings-dialog tests](../../../tests/ui/qml/tst_SettingsDialog.qml), graph presentation tests and integration fixtures provide starting seams.


| Requirements            | Design element                                                                                     | Verification approach                                                                                                                                                                    |
| ----------------------- | -------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SCOPE-01–04; UI-01–06 | Graph-keyed capabilities, popup, contextual navigation, Graphs page and Graph Lines delegation     | Qt Quick coverage across all five graph keys, correct control omission, preserved Graph Lines entry points and no preview                                                                |
| UI-07; QUAL-01–02      | Scrolling/reflow, accessible labels, locale parsing and focus flow                                 | Qt Quick keyboard/focus and size/text-scale checks, localized valid/invalid dates and numbers, reachable footer                                                                          |
| RANGE-01–06            | Discriminated ranges, relevant-run metadata and injectable UTC day source                          | Pure/application tests for inclusive endpoints, suffix order, both anchors, rollover, new runs, non-improving benchmark runs and absent anchors                                          |
| RANGE-07–09            | Full-data calculation then projection, original run keys and rank carry context                    | Compare overlapping values before/after filtering; assert original run selection, no out-of-range marker, independent benchmark axes and unchanged summaries                             |
| AVG-01–03              | Parameterized full-history playtime series and effective labels                                    | Domain/application comparisons with existing gap/initial-window accounting, one-day totals, independent windows and dynamic presentation labels                                          |
| STYLE-01–07            | Shared painter, actual observations versus geometry, series overrides and separate tier references | Geometry/painter and presentation tests for each mode, widths, singletons, dense Points only, marker restoration, non-selectable carry/spline points and unchanged tier references       |
| AXIS-01–04             | Visible-data shared-axis policies, exact displayed-unit bounds and capability exclusions           | Axis/presentation tests for visible members, percentages/minutes, fixed clipping, singleton/empty fallback and absent run/rank fitting controls                                          |
| STATE-01–08; SCOPE-03  | Committed service, one presentation draft, complete validation/save and scoped reset               | View-model and integration tests for immediate quick changes, multi-graph/Graph Lines isolation, navigation, baseline after Save, close choices, reset preservation and migration/reopen |
| EDGE-01–02, 05         | Raw-input retention and complete-candidate validation; unchecked QSettings submission              | Invalid edits on a non-selected graph block the whole Save; valid candidates reach the writer without durability checks; rejection leaves accepted memory and other edits unchanged      |
| EDGE-03–04; EDGE-06    | Distinct content/projection states and always-present gear                                         | Empty/no-history/unavailable fixtures, missing relevant anchors, rank carry without observations, reachable recovery controls and preserved completeness gates                           |
| QUAL-03–04             | Presentation-only configuration and bounded refresh/cache invalidation                             | Integration comparisons of source records, IDs, definitions and totals; representative-profile recalculation checks with selection/draft retention                                       |

Ordinary isolated QSettings round-trip tests establish codec and restart behavior under normal writes; they are not a write-success verification mechanism in the product. Persistence-failure injection is explicitly outside this feature's agreed acceptance checks.

During implementation, use the repository's scoped `uv run python scripts/build_and_test.py --scope <scope>` workflow and finish with the unscoped wrapper. This documentation proposal does not run or claim those implementation gates.

## Architectural decisions

The developer adopted these choices for this proposal:

- Keep QSettings and treat valid write submission as saving, without checking durability. The proposed atomic external JSON file and added persistence-failure handling were declined.
- Put the complete graph/Graph Lines working copy in SettingsViewModel and leave committed state as the dashboard's source. Store-owned shared draft mutation is replaced.
- Carry established benchmark rank into selected date windows in line modes; Points only and tooltips retain actual improvement observations.
- Use a unified versioned configuration document, a data-owned committed service, application use-case boundaries, full-history-derived projections, shared painting and initial synchronous cached calculation, as confirmed in the design preview.

ADR 0001 and ADR 0004 govern contract placement and application access. ADR 0005 remains authoritative for benchmark state; the new configuration service does not take ownership of benchmark definitions, drafts or profile reconciliation.

**ADR recommended:** Record unified graph-configuration ownership, presentation-owned drafts and the complete-snapshot application boundary. This replaces an established cross-component editing pattern and will guide future settings features. The choice and its consequences are fully specified here; no new ADR is required for reviewing this proposal, and none is created by this workflow.

## Recommended realization slices

### 1. Configuration and draft lifecycle — enabling increment

**Outcome:** Existing Graph Lines editing is isolated until Save and the complete configuration can be seeded, validated, saved, discarded and reloaded through QSettings. **Subset:** committed service, persistence migration, application boundary and SettingsViewModel working copy, including axis-label selection. **Deferrals:** new range controls and rendering choices need not yet be exposed. **Dependencies:** preserve legacy codecs, stable allocators and existing editor capabilities. **Main uncertainty:** adapting current store-mutating editor calls without a second accepted-state cache. **Completion evidence:** migration and isolated multi-section draft tests, plus existing Graph Lines regression coverage.

### 2. History ranges and averaging — data capability increment

**Outcome:** Each history type can produce its own requested visible projection and the two playtime windows can vary independently without changing full-history meaning. **Subset:** range model, relevant anchors/day source, original-run identity, separate benchmark axes, carried rank context and effective labels. **Deferrals:** final popup layout, appearance controls and accessibility polish. **Dependencies:** committed configuration and full-data query contracts. **Main uncertainty:** benchmark projection metadata and cache separation for non-improving contributing runs. **Completion evidence:** stable overlap values, fake-day rollover, unchanged summary totals and selection, and carried rank with no fabricated observations. This slice becomes a user-facing increment when exposed by slice 4.

### 3. Rendering and axes — presentation capability increment

**Outcome:** Configured graphs can render smooth, straight or point-only data, apply per-series appearance, and honour eligible axis policies. **Subset:** painter/SeriesModel width transport, observation/geometry separation, numeric policies and clipping. **Deferrals:** complete editing surfaces. **Dependencies:** stable series/axis keys and visible projections. **Main uncertainty:** preserving existing defaults while removing synthetic hit-testable rank endpoints. **Completion evidence:** painter/axis tests for singleton and dense inputs, exact displayed bounds, independent marker preference, original tooltips and unchanged tier reference semantics.

### 4. Integrated configuration flow — product increment

**Outcome:** All five graphs support the complete gear-to-settings flow, immediate quick changes, cancellable multi-graph editing, reset and understandable empty states. **Subset:** shared popup, contextual graph selector, grouped detailed controls, Graph Lines link, inline errors, keyboard access and scrolling/reflow. **Deferrals:** none of the agreed feature behavior; additional spline algorithms, per-data-item profiles and preview remain outside scope. **Dependencies:** prior capabilities. **Main uncertainty:** usability at the supported settings dimensions and representative profile sizes. **Completion evidence:** Qt Quick/integration flow coverage, responsiveness checks, full regression wrapper and the owner's separate product walkthrough.

## Risks, trade-offs, and implementation latitude

- Moving Graph Lines out of a shared store draft changes its notification and mutation paths. Keeping compatibility adapters is acceptable only while there remains one accepted authority and one editor working copy.
- A unified document couples upgrades to Graph Lines and graph preferences. Migrations must preserve definitions, axes, allocator exhaustion and legacy selector choices; invalid new preferences must not erase valid old sections.
- QSettings submission without verification intentionally cannot promise durable success or rollback after an underlying write failure. Product behavior must not later imply a stronger persistence guarantee than the agreed requirements.
- Independently configured benchmark axes remove an existing shared-axis convenience. Matching default extents reduce upgrade surprise; selected ranges thereafter remain independent.
- A rank line without in-range improvement markers is intentional historical context, not new activity. Its empty-point state and series name must make that interpretation understandable.
- Cache invalidation and synchronous calculation require profile-scale evidence. No numeric latency target or measured performance improvement is claimed.

Implementation may choose concrete helper names, contract decomposition, cache containers, exact shortcut presets, gear placement, accessible control composition, colour-picker layout and positive width limits consistent with existing validation. It may not relocate draft ownership, add unchecked fabricated observations, change day/accounting semantics, add persistence-verification machinery, or silently introduce asynchronous calculation. Material changes to those decisions return for design review.
