---
type: fixed
area: Settings
user: Long paths in Settings > Profile now wrap so the whole path stays readable, and the profile rebuild button is now labelled "Rebuild Profile from Kovaaks Directory".
---
`DirectoryPickerRow.qml`: the path was a read-only single-line `TextField`, which hides whichever end of a long path is scrolled out of view (the start, `C:/Users/...`, was cut off, and the full path was only reachable through a hover tooltip). It is now a read-only `TextArea` with `wrapMode: TextEdit.WrapAnywhere`, `selectByMouse`, a "Not set" placeholder and the field label as its accessible name, so the whole path (including the final folder or file name) is always on screen and still selectable for copying. `objectName`s (`kovaaksDirField`, `profilePathField`) and the `text` contract are unchanged; the Browse button is top-aligned so it stays beside the first line.
`SettingsDialog.qml`: `generateProfileButton` text changed from "Generate Profile from current kovaaks dir" to "Rebuild Profile from Kovaaks Directory", which matches the Title Case of the other buttons, spells "Kovaaks" and "Directory" like the field label above it, and says what the action does to an existing profile.
Covered by `tst_SettingsDialog.qml` (`test_longPathsWrapSoTheWholePathIsVisible`, `test_generateProfileButton_labelIsClearAndConsistentlyCapitalised`).
