---
type: added
area: Graphing
user: Choose which dashboard graph lines are enabled; disabled lines leave the control panel and View menu without losing their visibility choices.
---
`GraphLineConfig` persists disabled stable column keys through an opaque lower-layer interface, while `GraphColumnPreferences` owns known-column semantics and preserves unknown keys across updates.
`GraphViewModel::allColumns` supplies the Settings editor and `enabledColumns` is the single observable QML read path. Dashboard drawing, visibility controls, the View menu, and y-axis selection intersect enabled state with the existing independent visibility settings.
`App` injects graph-line configuration as a deterministic leaf service and observes it with a weak preference capture, avoiding a retained config/preferences ownership cycle.
