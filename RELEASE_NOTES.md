# Release Notes

What's new for people using the app. For the technical record, see [CHANGELOG.md](CHANGELOG.md).

## v0.6.0-beta

### Features & changes

#### Graphing
- Added option to change the alpha value of series colors
- Graph-series settings now persist safely across launches.
- Build and edit graph-line expressions with a visual tree editor, and manage graph lines in place — reorder, rename, recolor, edit expressions, add, or delete them. Edits are previewed as a draft and only applied when you confirm, or discarded if you cancel.
- Custom graph lines can now share a Y-axis with each other and with built-in series, instead of always getting their own independent axis.
- Main graph series are now resolved through the persisted series configuration and presented with stable identities.
- Graph data now loads only enabled series and no longer exposes legacy graph mutation controls.
- Graph line enablement is now separate from per-line dashboard visibility, and visibility has its own temporary control distinct from whether a line is enabled.
- Series configuration now uses the app-wide settings service.
- Scenario-history column visibility and Y-axis choice reset to defaults once, due to an internal settings-key format change.

#### Settings
- Series configuration can now be managed through the settings view model.
- Graph line changes in Settings now stay pending until you hit Save or Discard — the dashboard graph updates live as you edit, but nothing is written to disk until you confirm.
- Series configuration edits now use one unified store update operation.

#### User Interface
- Series configuration rows now use a contrasting background, a brighter six-dot grip, hover highlights, and compact expression-editor controls to make their editing affordances clear.

### Bug fixes
- fixed a bug where the history graph would always render a y-axis even against no data
- Several graph lines (Kills, Dmg, Score Total, Expected Final Score, and the "(5s)" variant) were rendering under the wrong name/color or not at all.
- Added alpha support to the hover info box
- The expression editor dialog now resizes itself to fit the expression you're editing, and scrolls instead of clipping when it's too big for the window.
- The graph line list in Settings no longer pushes the Save/Discard buttons off-window when you have a lot of series — it scrolls instead.
- Reopening Settings after saving graph line changes now lets you edit and save again — previously every further edit persisted immediately and the Save/Discard buttons stayed disabled.
- Fixed a bug where reordering a graph line by dragging it could throw an error and leave the series list rendering rows on top of each other until you dragged something else.
- Fixed a crash when editing certain deeply nested computed-series expressions. Unfortunately this required removing the nested cards layout. Now they are listed.
- Fixed a crash that could happen right as a scenario run's stats file appeared on disk.
- Fixed the app failing to find your KovaaKs install when no directory had ever been explicitly configured.
- The app no longer opens a separate terminal window alongside its main window.
- Hiding a graph line now stays hidden after restarting the app.
- The Help > About dialog now shows the app name and version instead of a placeholder.

## v0.5.1-beta

### Features & changes

#### Scenario & run selection
- Personal-best runs are now marked with a PB badge in scenario and recent-run lists.

#### Graphing
- Score Total, Expected Final Score, and Expected Final Score (5s) now share a y-axis, so you can compare their actual values.

### Bug fixes
- A failed or interrupted save can no longer corrupt your profile — the previous store is left intact.
- Run graphs no longer open on a spurious zero-value point at 0s, which was also dragging the y-axis down to 0 unnecessarily.
- Expected Final Score and Expected Final Score (5s) now project accurately throughout a run instead of consistently landing under the real total, especially on scenarios that manipulate time flow.
- The y-axis picker in the Graph Lines settings now lists one entry per axis, so columns that share an axis (Score Total, Expected Final Score, Expected Final Score (5s)) no longer show up as separate, functionally identical choices.

## v0.5.0-beta

### Features & changes

#### Scenario & run selection
- Scenario list can now be sorted by run count, name, or last played, via a new sort control next to the search field.
- Moved the sort selection next to the search bar & changed the default from "Runs" to "Last Played"
- Duration sorting has been removed from run lists.
- Removed a few controls from the control panel that never actually worked (a scenario filter dropdown, an empty display-mode dropdown, and a "Rendering" label that never updated).

#### Graphing
- New scenario history graph, plotting score, accuracy, shots, hits and misses across every run of the scenario you're currently viewing.
- Choose which dashboard graph lines are enabled; disabled lines leave the control panel and View menu without losing their visibility choices.
- You can now choose which metric's y-axis is labelled on the scenario graph, from Settings → Graph Lines. The choice is remembered across launches.
- The scenario graph's y-axis now shows a small rotated title naming the metric its tick numbers belong to, so it stays clear even after switching the axis metric in Settings.
- The graph's y-axis title now sits snug against the tick numbers instead of leaving a fixed gap, and adjusts automatically whether the numbers are short or long.
- Playtime dates now use calendar-aligned graph bounds and tick marks instead of arbitrary epoch-day intervals.

#### Settings
- The "Generate Profile" button now lives in Settings, under the renamed "Profile" category. Also removed the "Have Graph Load Latest Performance File" button.

#### User Interface
- New first-run banner prompts you to choose your Kovaaks folder before viewing statistics.
- The run currently shown in the graph is highlighted in both run lists.
- New View menu lets you hide the Scenario Graph, its individual lines, the Playtime Graph, the Control Panel, and the Selection Panel (with its Recent Runs and Scenario Browser sections) — your choices are remembered between launches.
- The app now has one consistent dark theme throughout. Previously the controls and the surfaces around them were drawn from two different colour schemes, which was most visible in the Settings dialog, and the controls followed the Windows light/dark setting while everything else stayed dark.
- "Load performance File..." moved from the sidebar into File > Load Performance File... in the menu bar.

#### Under the hood
- Profile generation now shows its progress while your KovaaKs runs are scanned.
- Generating a profile no longer freezes the app — the window stays responsive while your runs are scanned, and the results appear when it finishes.
- Profiles can ingest runs from multiple configured KovaaKs directories. Existing profiles are rebuilt from disk on first launch, so runs whose `.perf` files are gone will not survive the upgrade.
- The default profile file is now profile.pb; existing installs using profile_cache.pb must select it again in Settings.
- Release builds no longer include the development component gallery.

### Bug fixes
- The scenario panel no longer resizes every time you click a scenario or a run. Its width is set from your scenario list when a profile loads, and never exceeds a third of the window.
- Scenario run labels remain safe when the operating system cannot convert their timestamp to local time.
- "Have Graph Load Latest Performance File" now always loads your most recent run. Previously it only appeared to work before you'd selected anything else — once you picked a different scenario or run, clicking it just reloaded that selection instead of your latest run.
- The graph now shows your most recent run as soon as the app opens, instead of staying blank until you interact with something.
- The Settings dialog now opens centered on the window instead of pinned to the top-left corner.
- Run stats (shots, hits, misses, kills, damage, score) could be undercounted when a run had multiple events at the same timestamp — only the last event counted instead of the sum. Values now accumulate correctly.
- A profile file that cannot be read is now kept aside instead of being overwritten.
- File > Quit now closes the app, and Help > About shows a placeholder dialog. File > New (which did nothing) has been removed.

## v0.4.1-alpha

### Bug fixes
- New runs completed while the app is open are now correctly picked up and added to your stats — previously they were silently ignored until the app restarted.
- Changing your KovaaKs folder in Settings now takes effect immediately — previously new runs stopped being picked up until you restarted the app.
- Fixed a rare startup crash if a new run's .perf file appeared before the profile finished loading.

## v0.4.0-alpha

### Scenario & run selection
- New scenario search panel — search and browse scenarios instead of navigating them manually.
- Per-scenario run list, with sorting options for how runs are ordered.
- "Recent runs" quick-access list showing your latest runs across all scenarios at a glance.
- Various fixes and polish to the selection interface based on early use.

### Graphing
- New playtime graph, tracking play time alongside your performance stats.
- Reworked graph rendering for smoother lines and finer visual control, including improved curve interpolation.
- Graph axes now use "nice" tick positioning for more readable scales.
- Hover tooltips on graphs for inspecting individual data points.

### Under the hood
- Profile cache storage switched to a single file rather than a directory, simplifying data management.
