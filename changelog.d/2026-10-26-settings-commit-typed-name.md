---
type: fixed
area: Settings
user: Closing Settings, or pressing Save, while still typing a line name now keeps that name instead of silently dropping it.
---
A Graph Lines line name reaches `SettingsViewModel` only on its field's `editingFinished`, and closing the window doesn't emit it. So `SettingsDialog.onClosing` read `pendingChanges` as false, called `endVisit()` with no "Save changes?" prompt, and discarded the typed name. Save was also disabled (`enabled: pendingChanges`) until the field lost focus, and a disabled button takes no focus when clicked, so clicking it never committed the name either.
Fields that commit on `editingFinished` are now `CommitOnFinishField` (`qml/common/`), which reports `uncommittedEdit` while its text differs from `committedText`.
`SettingsDialog.commitActiveEdit()` emits `editingFinished` on the focused field when it holds such an edit. `onClosing` calls it before reading `pendingChanges`, and Save calls it before `save()`.
Save is also enabled while the focused field holds an uncommitted edit, and only then: a merely focused field, or a Graphs-page field (those report every edit as it is typed), leaves Save disabled.
