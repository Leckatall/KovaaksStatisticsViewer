---
type: fixed
area: Settings
user: Changing a line or the Y-axis choice now clears a settings error it fixes, instead of leaving it marked until another edit.
---
`SettingsViewModel::graphLinesChanged` (and so every Graph Lines edit and `setRunAxisLabel`) now revalidates the draft and drops the rejected Save's issues once none remain, through the new `settleIssues()` shared with `draftChanged()`.
