---
type: changed
area: User Interface
user: The bottom status bar now appears over the bottom of the window instead of pushing the content up, so messages no longer make the window content jump.
---
`ProfileStatusBar` is hidden when idle (no import status and no capture feedback) and is anchored over the bottom of `Main.qml`'s content instead of being the window `footer`, so the workspaces keep their geometry as it shows and clears. Mode `"hidden"` is now `"idle"`.
