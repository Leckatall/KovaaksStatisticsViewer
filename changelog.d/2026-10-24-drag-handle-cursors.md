---
type: changed
area: User Interface
user: Graph line drag handles show an open hand on hover and a closed hand while dragging.
---
`SeriesConfigEditorDelegate` adds a `HoverHandler` (open hand) on the handle and the `DragHandler` uses the closed hand. Per the Qt 6.11 docs `PointerHandler.cursorShape` applies only while the pointer is over the parent item, so the closed hand reverts if the pointer leaves the handle mid-drag.
