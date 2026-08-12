---
type: fixed
area: User Interface
user: File > Quit now closes the app, and Help > About shows a placeholder dialog. File > New (which did nothing) has been removed.
---
`AppMenuBar.qml`'s File > New, File > Quit and Help > About actions were dead — no `onTriggered`
handler at all. New is removed outright. Quit and About now follow the existing signal-out pattern
(`setSourceDirRequested`, `settingsRequested`): new `quitRequested()`/`aboutRequested()` signals fire
on trigger, and `Main.qml` reacts (`Qt.quit()`, opening the new `AboutDialog.qml`). Routing through a
signal rather than calling `Qt.quit()` inline keeps `AppMenuBar` testable — `tst_AppMenuBar.qml`
triggers every action including Quit, and a direct `Qt.quit()` call would kill the QML Quick Test
process. `AboutDialog.qml` is a bare placeholder `Dialog` (no version info yet), added to
`src/ui/CMakeLists.txt`'s `QML_FILES`.
