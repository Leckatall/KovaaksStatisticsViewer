---
type: changed
area: Graphing
user: Graph line visibility now has a separate temporary control from whether a line is enabled.
---
Replaced scattered QML settings with `VisualSettingsManager`; graph-line controls filter enabled series through its visible set while the settings dialog persists enabled-state changes through `SettingsViewModel`.
