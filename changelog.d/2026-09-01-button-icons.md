---
type: changed
area: UI
user: Copy, paste, delete, and the expression-editor (ƒx) buttons now show icons instead of text labels.
---
Four Lucide SVGs bundled via `RESOURCES` in the `ksv_ui` `qt_add_qml_module`, shown through each
`Button`'s `icon.source` (`display: IconOnly`), monochrome `currentColor` glyphs auto-tinted to the
palette text color. Buttons keep their meaning via `Accessible.name` + `ToolTip`. `Qt6::Svg` linked
into `ksv` so `windeployqt` bundles the `qsvg` image-format plugin. Touches
`SeriesConfigEditorDelegate.qml` (ƒx, delete), `ExpressionEditorDialog.qml` (copy, paste),
`PathCard.qml` (delete).
