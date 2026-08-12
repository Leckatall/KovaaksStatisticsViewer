---
type: changed
area: Settings
user: The "Generate Profile" button now lives in Settings, under the renamed "Profile" category. Also removed the "Have Graph Load Latest Performance File" button.
---
Moved `generateProfileButton`/`profileBuildProgressBar` from `ControlPanel.qml` to
`SettingsDialog.qml`'s first category, which is now labeled "Profile" instead of "Directories".
`SettingsDialog` gains a `required property var sessionVm`; `ControlPanel` drops it (no longer used).
Also removed `loadLatestPerformanceButton` from `ControlPanel.qml` entirely.
