---
type: changed
area: User Interface
user: Graph line drag handles show an open hand on hover and a closed hand while dragging.
---
`SeriesConfigEditorDelegate` adds a `HoverHandler` (open hand) on the handle and the `DragHandler` uses the closed hand.
`PointerHandler.cursorShape` only applies while the pointer is over the parent item, so `SeriesConfigDraftPanel` adds a full-size overlay `Item` with a `HoverHandler` (closed hand), enabled only while a row is dragged, to keep the closed hand once the pointer leaves the handle.
The overlay is parented to the `Pane` itself because a second child in its `contentItem` would change the panel's implicit size.
