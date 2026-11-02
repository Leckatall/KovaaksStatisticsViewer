---
type: changed
area: User Interface
user: The bottom status bar now always stays visible, so messages no longer make the window content jump.
---
`ProfileStatusBar` mode `"hidden"` is now `"idle"` and the bar no longer binds `visible` to it, keeping its fixed height. Gives capture feedback a stable place in the footer.
