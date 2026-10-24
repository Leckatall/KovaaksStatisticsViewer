---
type: fixed
area: User Interface
user: Settings and the Benchmark Manager now use the app's own colours instead of your Windows accent colour, and toggles stay readable while the window is focused.
---
`Theme.palette` holds the complete app palette (every role, plus disabled overrides); `Main.qml`, `SettingsDialog.qml` and `BenchmarkManagerDialog.qml` each set `palette: Theme.palette`.
Old mechanism: only `Main.qml` pinned `accent`/`highlight`, and an `ApplicationWindow`'s palette does not reach other top-level windows. Every other role, and every role in Settings and the Benchmark Manager, came from the Windows platform theme, which derives highlight/accent from the user's Windows accent colour.
The Windows theme also gives Switch its own per-control palette whose active-group `button` is the accent; Fusion's SwitchIndicator paints the handle and groove from `button` and the checked fill from `highlight`, so while the window was focused on and off were both accent-coloured and nearly indistinguishable. An explicit window palette overrides that per-control palette.
`ksv_driver` no longer writes its own snapshot of the Windows dark palette into each window, so headless screenshots now show exactly what the app ships. `alternateBase` is now a neutral `#343434` rather than the accent-derived blue in that snapshot.
