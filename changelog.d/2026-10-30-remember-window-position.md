---
type: added
area: User Interface
user: The main window now reopens where you last left it.
---
`VisualSettingsManager` persists `x`/`y` plus a `hasPosition` flag in the `"window"` `Settings` category; until a position is saved the OS keeps placing the window.
`Main.qml` is created hidden and shows itself at the end of `Component.onCompleted`, after restoring the position: a window shown during creation is placed by the OS first, and `onXChanged` would record that placement over the saved position before it was read.
The position is restored only when `isVisibleOnScreens` finds enough of the window's top edge on a current screen, so a position saved on an unplugged monitor falls back to OS placement.
Geometry (including the existing width/height) is recorded only while `visibility === Window.Windowed`, so maximising no longer overwrites the saved size and a minimised window's -32000 parking position is never saved.
