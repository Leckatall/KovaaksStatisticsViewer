---
type: internal
area: User Interface
---
`--scope qmllint` is clean: every warning-level finding across `src/ui/qml/` is fixed (453 → 0; info-level findings remain).
Every QML file with delegates now declares `pragma ComponentBehavior: Bound`, and each delegate declares `required` properties for the model roles it reads instead of relying on implicit context injection.
`series/FieldEditors.qml` stays unbound: its inline components are instantiated from `ExpressionTreeEditor.qml`, and a Bound inline component cannot be instantiated from another file. `BinaryOpEditor` instead maps its operator kinds to `{text}` model entries with `textRole`, dropping the custom delegate that needed outer-scope `kindLabel`.
Ids that shadowed a same-named property were renamed (the property, being API, kept its name; objectNames unchanged): Main `visualSettingsManager`, BenchmarkEditorTable `identityTableView`/`thresholdTableView`, GraphSettingsEditor `editorScroll`/`seriesColorDialog`, SeriesConfigDraftPanel `axisCreationDialog`/`seriesOptionsPopup`, SettingsDialog `unsavedChangesPrompt`.
`BenchmarkCellEditor.cell/table`, `BenchmarkRankHeaderCell.table` and `ProgressGridRow/ProgressGridGroupCell.grid` are `var`, not their concrete component types: the QML tests import `src/ui/qml/benchmark` by file URL, which yields a second type identity for `BenchmarkEditorTable`/`BenchmarkProgressGrid` that the module's copies refuse to assign.
`GraphViewModelBase` is `QML_ANONYMOUS` and `GraphViewModel`/`PlaytimeGraphViewModel`/`BenchmarkHistoryViewModel` are `QML_ELEMENT`+`QML_UNCREATABLE` so qmllint can resolve `GraphCanvas.graphVm` and their enums; `GraphCanvas`'s `Q_PROPERTY` spells the type fully qualified because the generated qmltypes otherwise records an unresolvable relative name. qmltyperegistrar includes headers by bare filename, hence the new `presentation/graphs` and `presentation/benchmarks/tracking` include dirs on `ksv_ui`. The `qmlRegisterUncreatableType` calls in `declare_metatypes()` are kept.
`WorkspaceTabButton` moved out of the `ColumnLayout` to the window root, since being declared lexically inside a layout made its intentional fixed `width` look layout-managed.
