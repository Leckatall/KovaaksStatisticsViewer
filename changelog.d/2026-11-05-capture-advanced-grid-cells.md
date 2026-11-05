---
type: internal
area: User Interface
---
The Advanced options grid in `CapturePreviewDialog.qml` hid its width and height error labels while empty. `GridLayout` removes a hidden item from its cells, so the Height label slid into the width row's third column and the Height field wrapped onto the next row. The error labels now stay visible (empty, they add no height beyond the field's row), keeping the three-column layout fixed.
