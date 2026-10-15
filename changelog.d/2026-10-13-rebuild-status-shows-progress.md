---
type: changed
area: Settings
user: While the profile is being rebuilt, the Profile page's status line says "Rebuilding", and a label above the progress bar shows how many files are done out of the total.
---
`SettingsDialog.qml` Profile page: the `profileLoadedLabel` status reads "Profile status: Rebuilding" (amber dot) while `sessionVm.profileBuildInProgress`, and falls back to the real `settingsVm.profileLoaded` state ("Loaded" / "Not loaded") when the build ends; `profileLoaded` describes the profile as it stood before the rebuild, so it was misleading mid-build.
New `profileBuildProgressLabel` above `profileBuildProgressBar`, visible only during a build: "Scanning Kovaaks Directory..." until the first per-file report, then "Rebuilding profile: N of M files".
`SessionViewModel` exposes the already-tracked done/total counts as `profileBuildDone` / `profileBuildTotal` (both reset to 0 on build start/finish, same notify signal as the existing progress properties).
Tests: `session_vm_test.cpp` pins the counts; `tst_SettingsDialog.qml` pins the status text during and after a rebuild (with a notifying `QtObject` fake session VM) and the label text in both phases.
