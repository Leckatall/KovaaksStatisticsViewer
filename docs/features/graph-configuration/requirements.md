---
status: approved
---
# Graph configuration

## Summary and user outcome

Give users quick access to recent training data and control over graph presentation without permanently taking space away from their charts. Each graph has a gear button opening a small popup for frequent adjustments and a **More graph settings…** action leading to that graph's detailed settings.

Detailed settings use one graph selector and grouped controls for the selected graph. They contain no graph preview. Settings belong to the graph type and remain consistent when users change runs, scenarios, or benchmarks. Only applicable options appear.

The [agreed Excalidraw mockup](graph-popup-and-settings.excalidraw) illustrates the popup and settings layout. It is a layout reference, not a complete inventory of applicable controls; the requirements and applicability table below govern scope.

Requirement IDs combine a short group name with a number, such as `UI-01`, `RANGE-01`, and `AXIS-01`. Numbering starts at 01 within each group. IDs remain stable when wording or order changes; new requirements receive unused IDs and retired IDs are not reassigned. Acceptance checks use `AC01`, `AC02`, and so on, with checkboxes and references to the requirements they verify.

## Problem, context, and evidence

The project owner primarily wants to inspect recent activity without older history compressing the useful detail. They also want adjustable rolling windows and graph appearance. A persistent shared inspector was rejected because horizontal chart space is limited. They preferred a compact popup plus access to detailed settings, and explicitly rejected a settings preview.

The current playtime graph displays a three-day rolling average over the returned history. Scenario history uses original run numbers and exposes selection of individual runs. The existing Graph Lines settings already manage the run graph's series with a saved/discarded draft. Benchmark tracking includes separate personal-best average-rank and rolling-playtime charts; the rank chart has meaningful tier labels and reference lines.

These observations are supported by the current [playtime graph](../../../src/ui/presentation/graphs/playtime_graph_vm.cpp), [scenario history](../../../src/ui/presentation/graphs/completion_history_vm.cpp), [settings dialog](../../../src/ui/qml/shell/SettingsDialog.qml), and [benchmark history presentation](../../../src/ui/presentation/benchmarks/tracking/benchmark_history_vm.cpp). The [benchmark requirements](../benchmarks/requirements.md) establish retrospective rank interpretation and benchmark-specific playtime. This feature makes the playtime averaging window configurable while retaining the existing three-day default.

Problem evidence and the chosen interaction are based on the project owner's experience and explicit decisions. They are not findings from broader user research. The expected usability improvement remains to be evaluated in the implemented product.

## Outcomes and scope

- **O1 — Inspect relevant practice:** reach recent dates or runs, or a specific historical period, without changing stored training history.
- **O2 — Read the graph comfortably:** adjust applicable smoothing and presentation to make the plotted information easier to inspect.
- **O3 — Preserve chart space:** keep frequent adjustments near the graph and put detailed configuration in settings.
- **O4 — Keep configuration predictable:** make scope, persistence, cancellation, and recovery understandable across graphs.

Scope includes the run graph (`DashboardGraph` in the product discussion), daily playtime, scenario history, benchmark personal-best average-rank history, and benchmark rolling playtime. The two benchmark charts have distinct configurations. The feature does not introduce per-scenario or per-benchmark configuration profiles, new computed-series capabilities, additional spline algorithms, or a new series editor for DashboardGraph.

## Main journeys

### Inspect recent practice

The user opens a graph's gear popup, chooses a recent range, and sees the graph update. On playtime graphs they can also change the rolling-average window. Date presets can end today or at the most recent relevant recorded run, according to a saved toggle. The popup closes without undoing those changes.

### Configure several graphs

The user chooses **More graph settings…**. Settings open with the originating graph selected. The user changes its applicable controls, selects another graph, and makes further changes. Returning to the first graph retains its unsaved edits. Save applies the complete valid draft; Cancel discards changes from that settings visit. There is no preview in the settings page.

### Manage DashboardGraph's lines

The user can change DashboardGraph's whole-graph connection mode and point-marker visibility. For individual lines they follow a link to the existing Graph Lines section. There are no axis-fitting controls or duplicate individual-series editor for this graph in the new page.

## Observable requirements

### Coverage and configuration ownership

Trace: O1–O4.

- **SCOPE-01 — Graph coverage.** Provide the configuration entry point and detailed-settings selection for each graph type listed in scope, including each of the two benchmark charts.
- **SCOPE-02 — Applicable controls.** Show only the controls applicable to the selected graph, according to the table below. Omit groups that contain no applicable controls rather than presenting an empty or disabled section.
- **SCOPE-03 — Per-graph ownership.** Save configuration independently for each graph type. Reuse a graph's configuration across run, scenario, and benchmark selection changes; do not create configuration separately for each selected data item. Editing one graph does not silently alter another graph's configuration.
- **SCOPE-04 — Preserve the existing series editor.** Retain existing Graph Lines capabilities and their entry points. Do not add computed-series creation or expression editing to other graph types as part of this feature.


| Graph                                        | Date/run range                                          | Rolling-average window               | Y-axis fitting                                         | Series appearance                                | Whole-graph line mode and markers |
| -------------------------------------------- | ------------------------------------------------------- | ------------------------------------ | ------------------------------------------------------ | ------------------------------------------------ | --------------------------------- |
| DashboardGraph / run graph                   | None; its horizontal dimension remains elapsed run time | No new graph-level averaging control | Not offered                                            | Link to existing Graph Lines section             | Offered                           |
| Daily playtime                               | Date presets, all time, custom dates                    | Offered in calendar days             | Numeric fitting                                        | Playtime series                                  | Offered                           |
| Scenario history                             | Latest N runs, all runs, date presets, custom dates     | Not offered                          | Numeric fitting for its existing axes                  | Existing history series                          | Offered                           |
| Benchmark personal-best average-rank history | Date presets, all time, custom dates                    | Not offered                          | Retain the tier-based axis; no generic numeric fitting | Existing rank series, preserving tier references | Offered                           |
| Benchmark rolling playtime                   | Date presets, all time, custom dates                    | Offered in calendar days             | Numeric fitting                                        | Existing playtime series                         | Offered                           |

The rank-axis treatment is a compatibility assumption: tier names and reference levels remain meaningful rather than becoming generic numeric bounds. Per-graph ownership means the two benchmark charts can have different selected ranges; both retain all-history defaults when no preference has been saved.

### Entry points and settings layout

Trace: O1, O3, O4.

- **UI-01 — Gear entry point.** Use a gear icon at each graph to open its quick popup. Give it an accessible name identifying graph settings and a discoverable tooltip; a permanent “Options” text button is not part of the design.
- **UI-02 — Compact popup.** Limit the popup to applicable frequent adjustments: range selection, playtime averaging window, whole-graph line mode, and point-marker visibility. Detailed axis bounds and individual-series appearance belong in settings. Existing series visibility controls remain available in their existing locations.
- **UI-03 — Contextual settings link.** Put **More graph settings…** at the bottom of the popup. Activating it closes the popup and opens graph settings with the originating graph selected and its current effective values represented.
- **UI-04 — One graph editor.** Present a graph selector at the top of the Graphs settings page. Organize the selected graph's controls under Data & range, Axes, and Series & style as applicable. Whole-graph line mode and markers remain available even where individual-series editing is delegated elsewhere.
- **UI-05 — No preview or inspector.** Do not place a preview graph in settings or introduce a persistent dashboard inspector. Detailed controls remain usable without taking permanent horizontal space from dashboard graphs.
- **UI-06 — DashboardGraph delegation.** For DashboardGraph, show a clearly labelled link to the existing Graph Lines section instead of individual-series appearance controls. Do not show an Axes group. Navigating through that link preserves the settings visit and its draft.
- **UI-07 — Usable settings dimensions.** Keep graph selection, group labels, editing, Save, and Cancel reachable at the application's supported settings-window sizes and text scales. Allow settings content to scroll or reflow rather than clip controls.

### Visible data ranges

Trace: O1, O4.

- **RANGE-01 — Date choices.** On date-capable graphs, offer all time, recent calendar-day presets, and a custom inclusive start/end date range. Present the selected range clearly enough to distinguish a moving preset from fixed dates.
- **RANGE-02 — Run-count choices.** On scenario history, offer all runs and latest N runs, including user entry of a positive whole-number N. Select the newest N available runs for the current scenario and display them chronologically; fewer available runs simply show the available subset.
- **RANGE-03 — One active range mode.** Date ranges and latest-N-run ranges are alternatives. Do not apply an undisclosed intersection of both. All time/all runs restores the full applicable history.
- **RANGE-04 — Configurable date anchoring.** Provide a saved **End date ranges at latest recorded run** toggle in Data & range for date-capable graphs. Off makes recent presets end today; on makes them end on the calendar date of the latest recorded run relevant to that graph's dataset. Custom start/end dates remain explicit and are unaffected by the toggle.
- **RANGE-05 — Relevant latest run.** Resolve the latest-run anchor from the selected scenario's runs for scenario history, the selected benchmark's contributing runs for benchmark graphs, and the full applicable profile for daily playtime. A benchmark's latest improvement point does not substitute for its latest contributing run when later runs did not improve rank.
- **RANGE-06 — Moving ranges.** Relative date presets and latest-N-run selections follow newly available relevant data. Today-anchored presets follow the current calendar date while the app remains open. Custom date boundaries remain fixed.
- **RANGE-07 — Stable historical values.** Apply visible ranges without redefining historical measurements. A value remaining visible after a range change retains its value for the same averaging window and data. Playtime averaging can use preceding days outside the visible range, and benchmark rank continues to reflect the full preceding history. In line modes, carry an established rank into a selected period even when its last improvement predates that period. Carried segments add no selectable observations; Points only shows actual improvement points.
- **RANGE-08 — Preserve run identity.** Scenario-history filtering preserves original run numbers, point-to-run selection, and tooltip identity. Changing a range does not select a different run. A selected run outside the visible subset has no misleading marker attached to a different visible run.
- **RANGE-09 — Presentation scope.** Range changes affect the selected graph, not recorded runs, profile contents, benchmark attainment, summary totals, or another graph's range. In particular, independently configured benchmark date ranges do not silently overwrite one another.

### Rolling averages

Trace: O1, O2, O4.

- **AVG-01 — Adjustable day window.** Let users select a positive whole-number averaging window in calendar days for daily playtime and benchmark rolling playtime. A one-day window displays the underlying daily totals using the existing daily-playtime accounting rules.
- **AVG-02 — Independent controls.** Treat averaging window and visible range as separate settings. Changing one does not silently replace the other. Preserve the existing treatment of calendar days without runs and of days before the first available record.
- **AVG-03 — Accurate labelling.** Graph titles, series descriptions, and other displayed averaging labels reflect the effective window. A value other than three days is never described as a three-day average.

### Lines, markers, and series appearance

Trace: O2, O4.

- **STYLE-01 — Three connection modes.** Offer Smooth line, Straight lines, and Points only as whole-graph modes. Smooth line uses the existing spline algorithm. Straight lines connects consecutive data points directly. Points only draws the data points without connections. Additional spline algorithms are outside scope.
- **STYLE-02 — Marker behaviour.** Points only always displays point markers. In either line mode, marker visibility is an independent toggle. Returning from Points only to a line mode restores the user's previous line-mode marker preference.
- **STYLE-03 — Whole-graph scope.** Apply the selected connection mode and marker behaviour to all visible data series in the configured graph, including DashboardGraph. Do not reinterpret benchmark tier reference lines as data-series points or remove them when choosing Points only.
- **STYLE-04 — Individual-series appearance.** For graphs with local appearance editing in the applicability table, let users select an existing series and change its colour and line width. Changes affect that series in that graph. For a single-series graph, identify that series without requiring a redundant selection step.
- **STYLE-05 — No new measurements.** Connection mode and marker changes alter presentation only. They do not change recorded or computed values, tooltip measurements, benchmark results, or which run a data point identifies. Do not add selectable fabricated observations between actual data points.
- **STYLE-06 — Small datasets.** Keep a single available data point visible as a marker even when a line cannot be drawn and line-mode markers are normally off. A filtered history with one run remains inspectable instead of only reporting that a line needs two runs.
- **STYLE-07 — Semantic references.** Preserve benchmark tier reference colours, labels, and meaning when changing the rank data series' appearance. Identify the data series so a chosen series colour does not become an implicit statement of attained rank.

### Axis fitting

Trace: O2, O4.

- **AXIS-01 — Eligible numeric axes.** For numeric Y axes identified in the applicability table, offer Fit visible data, Start at zero, and Fixed bounds. Automatic fitting uses the applicable visible data; fixed bounds use the user's minimum and maximum. Date/run selection governs the horizontal extent separately.
- **AXIS-02 — Clear axis scope and units.** Where a graph has multiple axes, identify which axis is being configured and which existing series share it. Accept and display bounds in the units shown to users, including percentages and minutes where applicable. Preserve existing axis assignment and labelling capabilities.
- **AXIS-03 — DashboardGraph exclusion.** Do not offer axis fitting for DashboardGraph in either surface. Its existing Graph Lines axis-assignment and Y-axis-label controls remain governed by that editor; their presence does not imply new axis-fitting controls.
- **AXIS-04 — Rank compatibility.** Preserve the benchmark rank graph's tier-based labels and reference levels. Omit generic numeric fitting and fixed numeric bounds for that graph.

### Application, saving, cancellation, and reset

Trace: O4.

- **STATE-01 — Immediate popup changes.** Apply and persist each valid quick-popup change immediately. Closing the popup by dismissal or Escape retains those changes; no separate Save action is needed there.
- **STATE-02 — One settings draft.** Start detailed settings from the effective saved configuration. Keep all detailed graph edits in one draft for the settings visit. Switching graphs or settings categories preserves unsaved edits, and revisiting a graph shows its draft values. Draft edits do not affect the dashboard before Save.
- **STATE-03 — Save the complete draft.** Save validates, applies, and persists all graph changes made during that settings visit, not only those for the currently selected graph. Return the visit to a clean state after successful saving.
- **STATE-04 — Cancel the complete draft.** Cancel discards all unsaved edits across the settings visit. It restores the effective configuration from the start of the draft or its last successful Save. It does not undo popup changes committed before that draft began.
- **STATE-05 — Closing with unsaved changes.** Closing settings with pending changes offers Save, Discard, and Cancel closing. A Save rejected by validation keeps the settings and draft available. Discard closes without applying the draft; cancelling the close retains it. Persistence follows EDGE-05.
- **STATE-06 — Existing Graph Lines continuity.** Keep edits made through the existing Graph Lines section within the same settings-visit Save/Cancel behaviour. Navigation between the new graph page and that section does not drop drafts or independently commit unsaved graph changes.
- **STATE-07 — Reset scope.** Provide Reset this graph in detailed settings. It stages the selected graph's configuration defaults in the draft and follows the same Save/Cancel rules. It does not reset other graphs, delete computed-series definitions, or remove user-created axes from Graph Lines.
- **STATE-08 — Persistent preferences and defaults.** Restore saved settings on restart. Without a saved preference, preserve each graph's existing data extent, averaging window, and appearance, including three-day playtime averaging and all available history. New preferences do not require a profile rebuild.

### Invalid input, empty states, and recovery

Trace: O1, O2, O4.

- **EDGE-01 — Input validation.** Reject reversed or invalid date ranges, non-positive or fractional run counts and averaging windows, non-finite numbers, invalid line widths, and fixed bounds whose minimum is not less than their maximum. Show a useful issue next to the affected control rather than silently replacing the input with zero, clamping it to an unrelated value, or saving it.
- **EDGE-02 — Invalid draft and popup input.** Invalid detailed input blocks Save while retaining all draft edits for correction. Invalid quick-popup input leaves the last valid effective setting intact and permits correction without losing other valid popup changes.
- **EDGE-03 — Empty selected range.** When a valid range contains no observations, distinguish that state from a profile with no recorded history. Keep graph configuration reachable so the user can widen or clear the range. Do not silently switch to all history or fabricate activity to avoid an empty chart. An established rank carried into the range under RANGE-07 is historical state and remains drawable in line modes; Points only can have no improvement points in that same range.
- **EDGE-04 — Missing latest-run anchor.** With no relevant recorded run, retain the chosen anchoring preference and show the ordinary no-history state. Do not borrow an unrelated scenario's or benchmark's run to produce an apparently valid chart.
- **EDGE-05 — Preference persistence.** Use QSettings for presentation preferences. After validation succeeds, submit the valid change to QSettings and treat it as saved without checking write status or reading it back. Write-success verification, persistence-failure reporting, disk rollback, and persistence-failure simulation are outside scope. Validate the complete detailed draft before applying any of it; validation rejection retains all edits and leaves the effective configuration unchanged.
- **EDGE-06 — Unavailable graph content.** A graph without a selected run, with unsupported run data, or with unavailable benchmark history retains its meaningful existing empty state. Graph presentation preferences remain accessible without creating data or bypassing benchmark completeness rules.

### Compatibility and accessibility

Trace: O2–O4.

- **QUAL-01 — Keyboard and accessible operation.** Make gear buttons, popup controls, graph selection, settings groups, delegated links, Save/Cancel, and validation messages keyboard and screen-reader accessible. Preserve visible focus and allow users to leave the popup without a keyboard trap.
- **QUAL-02 — Readable controls.** Keep control names, selected values, units, errors, and the graph being edited understandable without colour alone. Date entry and display follow the application's locale conventions and existing calendar-day interpretation.
- **QUAL-03 — Training-data integrity.** Treat configuration as presentation preferences. Existing profile history, scenario identities, benchmark definitions, attained-rank interpretation, and computed-series definitions remain intact unless changed through their existing explicit editing flows.
- **QUAL-04 — Responsiveness.** Keep quick changes, graph switching, and detailed editing usable with profiles already supported by the app. Retain the user's selection and draft if a graph recalculation is delayed; do not replace settings with a blank or unusable state.

## Product constraints and risks

The current settings experience already has navigation and a series-editing draft. The new graph page extends that experience. It does not replace the existing series-management product with a second editor.

Filtering can accidentally change a rolling average's left edge or reset the meaning of personal-best rank. RANGE-07 treats these as display changes over the authoritative measurements. Likewise, filtering scenario history can otherwise break point selection; RANGE-08 preserves original run identity.

The current benchmark charts share a date extent. Independent graph preferences deliberately allow users to choose different extents; this is a presentation change, with matching all-history defaults retained. Benchmark calculation rules remain governed by the benchmark requirements.

Numeric fitting is inappropriate for DashboardGraph in this scope by explicit product decision. The tier-axis exception for benchmark rank preserves existing product meaning. Existing defaults reduce surprise for users upgrading without having configured the new controls.

The mockup omits some applicable quick controls for brevity. It also uses explanatory annotations outside the depicted application. Those annotations are not additional application copy or controls.

## Evidence of product success

The feature succeeds when the project owner can inspect their recent practice using the popup, make the desired presentation adjustments, and return to analysis without giving up permanent chart width or repeatedly re-entering preferences.

Useful evaluation is a walkthrough with the owner's real profile: inspect recent playtime, inspect a scenario's latest runs, adjust line presentation, and configure more than one graph in a single settings visit. The owner can explain which graph an edit affects and what Save or Cancel will retain. This is qualitative product evidence, distinct from the functional checks below. No time-saving percentage, adoption target, or broader usability result has been measured or claimed.

## Acceptance criteria

Each check is traceable to requirement IDs; the requirements remain authoritative for details not repeated here.

- [x]  **AC01.** Every in-scope graph has an accessible gear popup and can open detailed settings with the correct graph selected. A run graph has no date-range or axis-fitting controls; empty groups and the settings preview are absent. _Requirements: SCOPE-01, SCOPE-02, UI-01, UI-02, UI-03, UI-04, UI-05, UI-06._
- [x]  **AC02.** With old and recent records, relative dates differ predictably when the anchoring toggle changes. An explicit custom range does not move; latest N returns the newest available scenario runs in their original order. New records and a calendar rollover update moving selections appropriately. _Requirements: RANGE-01, RANGE-02, RANGE-03, RANGE-04, RANGE-05, RANGE-06._
- [x]  **AC03.** Narrowing a range leaves overlapping averaged values and personal-best rank values unchanged. Line modes retain an established rank when its last improvement predates the selected period, without adding selectable observations; Points only shows actual improvement points. Scenario-history points still select the original runs, and an out-of-range selected run does not mark a substitute. Another graph's extent and summary totals remain unchanged. _Requirements: RANGE-07, RANGE-08, RANGE-09._
- [x]  **AC04.** Both playtime graphs accept a changed whole-day window, display the effective window in their labels, and show daily totals at one day. Other graph types do not acquire a rolling-average control. Changing the visible extent alone does not reset the window. _Requirements: SCOPE-02, AVG-01, AVG-02, AVG-03._
- [x]  **AC05.** On every graph, Smooth line uses the existing curve, Straight lines directly connects observations, and Points only displays markers without data-series connections. Marker preference returns when leaving Points only; a single observation is inspectable. Reference lines, tooltip values, and run identities retain their meaning. _Requirements: STYLE-01, STYLE-02, STYLE-03, STYLE-05, STYLE-06, STYLE-07._
- [x]  **AC06.** Eligible graphs allow their existing series' colour and width to change independently. DashboardGraph delegates individual-series management to Graph Lines, preserves that editor's capabilities, and retains whole-graph modes. Existing benchmark tier semantics remain intact. _Requirements: SCOPE-04, UI-06, STYLE-04, STYLE-07._
- [x]  **AC07.** Eligible numeric axes fit visible data, start at zero, or use valid fixed bounds in displayed units. Shared-axis scope is clear. DashboardGraph and benchmark rank omit generic numeric fitting. _Requirements: AXIS-01, AXIS-02, AXIS-03, AXIS-04._
- [x]  **AC08.** A popup change survives dismissal and restart. Detailed edits to multiple graphs and Graph Lines survive navigation but do not affect the dashboard until Save. Save commits the complete draft; Cancel and Discard drop it. Cancelling a close retains it. Earlier committed popup changes survive cancellation. _Requirements: STATE-01, STATE-02, STATE-03, STATE-04, STATE-05, STATE-06, STATE-08._
- [x]  **AC09.** Reset affects only the selected graph's draft, is cancellable, and leaves computed definitions and user axes intact. Saved configuration follows its graph across scenarios and benchmarks. An upgrade with no preference retains the existing defaults. _Requirements: SCOPE-03, STATE-07, STATE-08._
- [x]  **AC10.** Invalid fields identify their problem, block invalid commits, and preserve other edits. An empty valid range offers recovery through the gear. Missing data is not replaced with unrelated history. Validation rejection applies none of the detailed draft; valid changes are submitted to QSettings without write-success verification. _Requirements: EDGE-01, EDGE-02, EDGE-03, EDGE-04, EDGE-05, EDGE-06._
- [x]  **AC11.** At supported settings-window sizes and larger text scales, users can reach and operate the complete flow with a keyboard, understand errors and units, and dismiss the popup. Representative existing profiles remain usable and their source records and definitions remain unchanged. _Requirements: UI-07, QUAL-01, QUAL-02, QUAL-03, QUAL-04._

## Bounded assumptions and presentation details

These details fill in the agreed feature; they are not claims of explicit user validation.

- Suggested initial shortcuts are 7, 30, and 90 calendar days and 25, 50, and 100 runs, alongside all history and custom entry. Exact shortcut labels and counts can change without removing the required range modes or custom counts/dates.
- The new date-anchoring toggle defaults off. Existing all-history defaults remain in effect until the user chooses a limited range, so this default does not initially hide recorded history.
- Custom calendar dates include both endpoints and use the graph's existing day interpretation. Locale-aware presentation does not redefine how stored runs are assigned to calendar days.
- Benchmark date preferences belong to the two graph types, not each benchmark definition. Changing the selected benchmark resolves the saved preferences against that benchmark's data.
- The numeric fitting choices and tier-axis exception make the mockup's general Axes group concrete while preserving specialised rank meaning. New axis-assignment features, tick-density controls, logarithmic scales, and free-form axis labels are outside this scope.
- Reset restores presentation defaults; it does not edit or delete the content managed by the existing Graph Lines editor. Exact colour-picker presentation and accepted positive line-width limits can follow the application's established controls and validation.
- A user-selectable dash pattern is outside this scope. Colour, width, markers, and the three connection modes are the initial appearance capabilities.
- The exact placement of the gear and compact arrangement of quick controls can follow each graph's available space. The graph selector and settings groups express the accepted structure; the drawing's pixel dimensions do not constrain implementation.
