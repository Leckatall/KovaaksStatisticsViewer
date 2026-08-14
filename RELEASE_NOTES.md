# Release Notes

What's new for people using the app. For the technical record, see [CHANGELOG.md](CHANGELOG.md).

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
