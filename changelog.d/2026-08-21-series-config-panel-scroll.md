---
type: fixed
area: Settings
user: The graph line list in Settings no longer pushes the Save/Discard buttons off-window when you have a lot of series — it scrolls instead.
---
`SeriesConfigDraftPanel.qml` — series rows now scroll independently, keeping the add, error, and Save/Discard controls visible; dragging near the list edge auto-scrolls while preserving the reorder target.
`SettingsDialog.qml` — the series-config panel now receives the Graph Lines page's available height.
