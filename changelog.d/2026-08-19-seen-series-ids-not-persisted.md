---
type: fixed
area: User Interface
user: Hiding a graph line now stays hidden after restarting the app.
---
`VisualSettingsManager.seenIds` tracked which series ids had already been synced so a newly-appearing
series defaults to visible while a previously-hidden one stays hidden, but it lived on a plain
`property var` rather than inside a `Settings` block, so it reset to an empty `Set()` on every process
start. `syncVisibleSeriesIds()`, called from `Main.qml` on `Component.onCompleted`, then saw every
enabled series as "new" and pushed it back into `visibleSeriesIds`, overwriting whatever the user had
hidden last session.
Replaced it with `seenSeriesIds`, a `Settings`-backed array (`graphSeriesVisibility` category,
alongside `visibleSeriesIds`) that persists the same way.
