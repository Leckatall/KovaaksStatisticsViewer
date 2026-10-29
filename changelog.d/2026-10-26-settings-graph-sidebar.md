---
type: changed
area: Settings
user: Settings lists each graph in the sidebar, with all of its settings on one page.
---
`SettingsDialog` presents graph-first navigation and per-graph change and issue markers using `SettingsViewModel::changedGraphs`.
`GraphSettingsEditor` hosts each graph's sections in one scrolling page; the Run graph's series and axes panels use its external scroller.
`GraphNumberField` inputs take a font-based preferred and maximum width rather than the containing layout's width.
