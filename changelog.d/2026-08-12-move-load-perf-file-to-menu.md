---
type: changed
area: User Interface
user: "Load performance File..." moved from the sidebar into File > Load Performance File... in the menu bar.
---
`AppMenuBar` gains `loadPerformanceFileRequested()` signal and a `&Load Performance File...` action,
following the same signal-out pattern as `setSourceDirRequested`/`settingsRequested`. `Main.qml` now
owns the `FileDialog` (`perfFileDialog`) and wires it to the signal, calling `graphVm.fetchData(...)`
on accept. Removed `loadPerformanceFileButton` and its `FileDialog` from the deprecated
`ControlPanel.qml`, along with the now-unused `QtQuick.Dialogs` import there.
