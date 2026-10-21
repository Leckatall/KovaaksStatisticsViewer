---
type: changed
area: Data & Profile
user: Profile refresh status now appears in a slim bar at the bottom of the window, and only when there is something to report. Settings → Profile shows Refresh and Rebuild side by side, lists problem files by name, and asks before rebuilding.
---
`ProfileStatusBar.qml` — new `ApplicationWindow.footer` with modes busy/warning/notice/hidden. The notice timer starts on the busy true→false edge, a dismissed warning resets on each completed import, and only issues with `previouslyFailed !== true` raise the warning. Replaces `ProfileImportStatus.qml`, which is removed. `SettingsDialog.openProfile()` opens the Profile page (the bar's Details… action); Rebuild from Scratch now goes through `rebuildProfilePrompt`.
