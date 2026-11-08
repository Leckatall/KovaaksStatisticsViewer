---
type: internal
area: Graphing
---
Capture sources and value snapshots under `src/ui/capture/`: `captureGraph` copies `GraphCanvas::scene()` without the selected-run band, `captureTable` copies the benchmark presentation, and per-target functions build graph context strings.
`captureAppearanceFromTheme` reads `Theme.qml` by property name for Stage 3 to call once from QML. `readableColorOn` and `isLightFill` port the Theme rules to C++.
`ProgressGridScenarioRow` gains typed presence flags, and `ProgressGridRow.qml` reads `hasPersonalBest` instead of comparing against the placeholder dash.
`GraphSceneAxis` carries formatted bounds; a series' own axes are now bounds-only, with no tick labels. `Axis::calendarRangeMs` reports the range a `DateTimeAxis` was given, and `CompletionHistoryViewModel` retains its visible run span and start times.
