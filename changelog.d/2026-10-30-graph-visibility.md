---
type: added
area: User Interface
user: Any graph can now be shown or hidden, from its Settings page, the new View → Graphs menu or its gear menu, with Undo straight after hiding one from the gear. Benchmark graphs can be hidden too.
---
Visibility lives in `VisualSettingsManager`'s `view/*` settings (three new benchmark keys), not `GraphConfiguration`: it applies immediately and Save, Cancel and Reset never touch it. `isGraphVisible()`/`setGraphVisible()` address it by `GraphCatalog` key.
`GraphSettingsEditor`'s title row became a status card with the Show graph switch; Reset to defaults moved to the first section's heading row, inside the scroll view. `sectionInView` now reads each group's own `y`: `mapToItem()` notifies nothing, so the binding could keep pre-polish positions, which the new layout exposed on the benchmark rank pages as the sidebar highlighting the last section.
`GraphCatalog.ensureVisible()` now leaves an unsized view alone: focus reaches a just-opened page before its layout's first polish, and the zero-height view made even the first field look out of view, scrolling the page past its heading row. The old pinned title hid this; with Reset on that row it would have opened out of sight.
`GraphVisibilityMenu` backs both View → Graphs and the Choose graphs… button of `NoGraphsShown`, shown when a workspace has no graph left. It replaces the flat Scenario Graph, Scenario History and Playtime Graph View entries.
`GraphGear`'s `hideGraphRequested` follows the `moreGraphSettingsRequested` route to the workspaces. `hideNoticeGraph` drives `GraphHiddenNotice` and is deliberately unsaved; there is one at a time, so a second quick hide replaces it and only one Escape shortcut is ever enabled.
`BenchmarkTrackableBody` keeps explicit x/y placement: a hidden card collapses to no height, or to the notice's height while the notice holds its slot.
