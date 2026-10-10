---
type: changed
area: User Interface
user: The Scenarios / Benchmarks tabs are now larger and the active workspace is clearly marked, so they read as the app's main navigation.
---
`Main.qml` workspace `TabBar` restyled. The bar was Fusion's default ~21px strip stretched across the window, and the checked and unchecked `TabButton` differed by a barely visible shade, so it read like a table header.
New inline component `WorkspaceTabButton` (a `TabButton` with a custom `background` and `contentItem`): 42px tall, 15px label, fixed width of at least 150px set explicitly so `TabBar` stops stretching the tabs across the window and they sit left-aligned like destinations.
The checked tab takes the content colour, a bold full-contrast label and a 3px `palette.accent` underline; the unchecked tab is dimmed and gets a hover tint. A 1px accent ring is drawn on `visualFocus` so keyboard (Left/Right) switching stays visible.
The `TabBar` itself gets a darker `background` with a 1px `palette.mid` bottom border. `objectName`s (`workspaceTabBar`, `scenariosTab`, `benchmarksTab`), accessible names and the Left/Right key handling are unchanged. The driver's `type:TabButton` selector still matches because it walks the meta-object superclass chain.
Covered by `tst_Main.qml` (tab height, width, bold/colour/background distinction following the selection).
