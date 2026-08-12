---
type: added
area: Data & profile cache
user: Profile generation now shows its progress while your KovaaKs runs are scanned.
---
`IFileService::listPerfFiles()` separates discovery from decoding so `ProfileBuilder` can report
each completed file. `ProfileBuildWorker` coalesces those reports to percentage-sized UI updates,
and `SessionController` forwards build lifecycle and progress signals to `SessionViewModel`.
`ControlPanel.qml` binds a progress bar to the view model; it is indeterminate until the worker
knows the total and hidden after the final profile is applied.
