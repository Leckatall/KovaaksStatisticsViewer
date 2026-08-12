---
type: changed
area: Settings
user: The app now has one consistent dark theme throughout. Previously the controls and the surfaces around them were drawn from two different colour schemes, which was most visible in the Settings dialog, and the controls followed the Windows light/dark setting while everything else stayed dark.
---
The app ran on the Fusion style (`QQuickStyle::setStyle("Fusion")`) but the QML was written against
Material's attached properties, so hand-drawn labels and surfaces resolved to Material's default dark
palette while every real control was painted by Fusion.

Fusion takes its colours from the system palette, so the fix is to give it a palette and pin it:
`QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark)` in all three entry points
(`main.cpp`, `gallery_main.cpp`, `qt_test_main.cpp`), which overrides the OS setting and hands Fusion
its dark palette. `Main.qml` then sets `palette.accent`/`palette.highlight` — the app's cyan, and the
single place it is now defined — which propagate to every control and popup below.

`import QtQuick.Controls.Material` is gone from all six QML files that had it. `Material.foreground`
on a `Label` becomes nothing at all (the default is already `palette.windowText`); `accentColor` →
`palette.accent`, `dialogColor` → `palette.window`. The hardcoded surface colours went the same way:
`#121212` was a full-window `Rectangle` in `Main.qml` that `ApplicationWindow`'s own `palette.window`
background makes redundant, `#1E1E1E`/`#2A2A2A` on the graph panels became `palette.base`/`palette.mid`,
and `ScenarioRunItem`'s down/hover/normal ramp is now derived with `Qt.lighter(palette.base, …)`.

The profile status dot in `SettingsDialog.qml` stays literal (`#4CAF50`/`#E53935`) — there is no
palette role for semantic ok/error. Graph series colours (`graph_vm.cpp`, `playtime_graph_vm.cpp`,
`axis_painter.h`, `series_painter.h`) are data colours and were left alone.
