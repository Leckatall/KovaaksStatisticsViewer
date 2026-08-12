---
type: removed
area: Scenario & run selection
user: Removed a few controls from the control panel that never actually worked (a scenario filter dropdown, an empty display-mode dropdown, and a "Rendering" label that never updated).
---
`ControlPanel.qml` — removed `renderingLabel` (bound to `SessionViewModel::getCurrentPerfScenario()`,
a plain `Q_INVOKABLE` with no `NOTIFY`, so it only ever showed the value from panel creation), the
`scenarioComboBox`/`filterByScenarioCheckBox` pair (nothing read either outside of the checkbox
gating its own sibling combo box — real scenario filtering already goes through
`ScenarioBrowserViewModel` via `SelectionPanel`), and `displayScenarioModeComboBox` (no `model` was
ever assigned). `generateProfileButton`, the load-performance-file `FileDialog`, and
`loadLatestPerformanceButton` are untouched — all three still drive real behavior. Top-of-file
comment now reads `DEPRECATED: This component is deprecated and all functionality should be moved
from here`, replacing the stale "extract line toggles" TODO.
