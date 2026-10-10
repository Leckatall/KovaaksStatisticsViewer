# ksv_driver: screenshots and scripted UI input

A dev-only tool for agents and humans iterating on the QML UI. `ksv_driver` runs the real `App` wiring (real view models, real `Main.qml`) against a disposable data directory, headless by default, and serves a small automation protocol on `127.0.0.1`. `scripts/ui_driver.py` is the CLI: it starts the app, prints screenshot paths, and clicks, types, scrolls and inspects the item tree.

```
python scripts/ui_driver.py start --build     # build ksv_driver, launch a fresh headless session
python scripts/ui_driver.py shot              # -> .temp/ui-driver/default/shots/001-main.png
python scripts/ui_driver.py tree --depth 6    # named/interactive items with geometry
python scripts/ui_driver.py click benchmarksTab --shot
python scripts/ui_driver.py type "Pasu" --into scenarioSearchField
python scripts/ui_driver.py eval "settingsDialog.open()"
python scripts/ui_driver.py shot --window settingsDialog
python scripts/ui_driver.py stop
```

Read the printed PNG path to see the UI. (`uv run python ...` works the same where uv is installed.)

## Sessions

A session lives in `.temp/ui-driver/<name>/` (default name `default`, pick another with `--session NAME`):

| Path | Contents |
|---|---|
| `data/` | the KovaaK's dir the app scans (`FPSAimTrainer/…`), `profile.pb`, `benchmarks/` |
| `settings/` | every `QSettings` (SettingsService and QML `Settings {}`); nothing touches the real registry or `%APPDATA%` |
| `shots/` | screenshots, numbered |
| `app.log` | the app's stdout/stderr |
| `session.json` | pid and port, written once the app is ready |

- `start` wipes the session and seeds it (`--keep` reuses it); it stops a running session of the same name first.
- `restart` keeps data and settings and reuses the last `start` options. QML is loaded from `src/ui/qml` on disk, so **QML edits only need `restart`**; C++ edits need `restart --build`.
- A running session holds `build-agent/ksv_driver.exe` open, so stop it before a full `build_and_test.py` run (`restart --build` handles this itself).

`--dataset`:

| Name | Data |
|---|---|
| `rich` (default) | the gallery's synthetic profile (4 scenarios, a month of history) plus a two-category demo benchmark |
| `single`, `short`, `bunched` | the gallery's other synthetic profiles, plus the demo benchmark |
| `fixtures` | `tests/examples/*.perf` / `*.csv`, imported through the real ingestion pipeline |
| `empty` | an empty KovaaK's dir: no runs, no benchmarks |
| `first-run` | no KovaaK's dir configured, so the first-run banner shows. The app then falls back to the default Steam path, which it reads if it exists. |

Other `start` options: `--visible` (a real window, rendered by the native backend), `--size 1280x800`, `--compiled-qml` (the qrc module instead of `src/ui/qml`), `--timeout`.

**Headless rendering.** The headless run uses the `offscreen` platform with the software renderer, which is the same setup the QML tests use. The offscreen platform has no fonts and ignores the dark colour scheme, so `ksv_driver` points it at the Windows fonts and uses Segoe UI 9pt. It also writes the Windows dark palette into every window, and `driver_main.cpp` holds the snapshot of that palette. The result matches the real app closely. Use `--visible` when exact native rendering matters.

## Commands

Every command accepts `--session NAME` and `--json` (raw protocol result). Actions (`click`, `hover`, `drag`, `scroll`, `key`, `type`, `set`, `invoke`, `eval`, `resize`) also accept:
- `--shot`, to take a screenshot after the action;
- `--settle MS`, the wait after the action (default 120).

Any warnings the app logs during a command are printed as `app warning: …`.

| Command | What it does |
|---|---|
| `windows` | Top-level windows. Settings and Benchmark Manager are separate windows (`settingsDialog`, `benchmarkManagerDialog`). |
| `shot [--window KEY \| --item SEL] [--pad N] [-o PATH] [--all-windows]` | Saves a PNG and prints its path. `--item` crops the image to one item. |
| `tree [SEL] [--depth N] [--all] [--raw]` | Item tree. By default only named, id'd, texted or interactive items are shown. `--raw` shows every item; `--all` includes hidden items. |
| `find SEL [--all] [--limit N]` | Every match, with type, name, id, text and `@x,y wxh` window coordinates. |
| `click SEL` / `click --at X Y`, `dblclick`, `rclick` | Real synthesized mouse events at the item's center. `--offset DX DY` makes the point relative to the item's top-left; `--mods ctrl+shift` adds modifiers. |
| `hover SEL`, `drag FROM TO` (selector or `X,Y`), `scroll SEL --dy N` | Positive `--dy` scrolls down, one wheel notch per unit. |
| `key SEQ [--repeat N]` | Keys such as `Return`, `Escape`, `Tab`, `Down`, `Ctrl+A`, `F2`. They go to the focused window. |
| `type TEXT [--into SEL] [--clear] [--enter]` | Types into the focused item. `--into` focuses the item first. |
| `get SEL PROP`, `set SEL PROP VALUE` | Reads or writes a QML property (`text`, `currentIndex`, `Layout.fillWidth`, …). `VALUE` is parsed as JSON when it can be, otherwise taken as a string. |
| `invoke SEL METHOD [ARGS…]` | Calls a method, slot or QML function, or emits a signal (`open`, `close`, `clicked`). |
| `eval EXPR [--on SEL]` | A QML/JS expression. It is evaluated in `Main.qml`'s scope, so `root`, `settingsDialog` and `workspaceTabBar` are available; `--on` uses an item's scope instead. |
| `wait [SEL] [--prop P --equals V] [--expr E] [--gone] [--timeout MS]` | Polls until the condition holds. |
| `resize WxH [--window KEY]`, `logs [--all] [--warnings] [--clear]` | |
| `batch FILE\|-  [--keep-going]` | Runs one step per line, as either a CLI line (`click benchmarksTab --shot`) or a raw JSON request (`{"cmd": "find", "args": {...}}`). Lines starting with `#` are skipped. |
| `start`, `restart`, `stop`, `status` | Session lifecycle. |

## Selectors

```
[win:<window>/] segment ( " > " segment )*
segment := term ( "&" term )* [ "@" N ]
term    := <objectName glob> | name:<glob> | id:<glob> | text:<glob> | type:<glob> | acc:<glob>
```

- A bare word matches `objectName`. Globs use `*` and `?`, so `scenarioItem_*` works.
- `id:` matches a QML id from the app's own `.qml` files.
- `text:` matches `text`, `title` or `placeholderText`. `acc:` matches `Accessible.name`. Both are case-insensitive.
- `type:` matches the QML type or any base type: `type:Button`, `type:AbstractButton`, `type:ScenarioWorkspace`.
- `a > b` finds `b` among `a`'s descendants. `@N` picks the Nth match of a segment (0-based; `@-1` is the last).
- `win:settingsDialog/…` searches only that window. Window keys come from `windows`; the main window is `main`.

Matching rules:
- Hidden items, such as other `StackLayout` pages or closed dialogs, are skipped unless you pass `--all`.
- When one target is needed and both a control and its own label match (`text:Close`), the control wins.
- Otherwise an ambiguous selector fails and lists its candidates.
- A selector that matches nothing reports similar names and how many hidden items would match.

Prefer `objectName`s. `Main.qml` and the workspaces have many: `scenariosTab`, `benchmarksTab`, `workspaceTabBar`, `scenarioSearchField`, `manageBenchmarksButton`, `benchmarkSelector`, … Adding an `objectName` to a QML item you want to drive is cheap.

## Protocol

`ksv_driver --session-dir DIR [--dataset NAME] [--size WxH] [--compiled-qml]` listens on a random `127.0.0.1` port, which it records in `DIR/session.json`. Requests and responses are newline-delimited JSON:

```
{"id": 1, "cmd": "click", "args": {"selector": "benchmarksTab"}}
{"id": 1, "ok": true, "result": {"target": {...}, "pos": [960, 39]}}
{"id": 2, "ok": false, "error": "selector 'type:TabButton' is ambiguous: …", "details": {"candidates": [...]}}
```

The commands are the CLI's, under their protocol names: `ping`, `windows`, `tree`, `find`, `screenshot`, `click`, `hover`, `drag`, `scroll`, `key`, `type`, `get`, `set`, `invoke`, `eval`, `wait`, `resize`, `logs` and `quit`. `UiDriver::dispatch` in `ui_driver.cpp` lists every argument. The CLI client lives in `scripts/ui_automation/`.

## Layout

- `ui_driver.{h,cpp}`: scene walk, selectors, input, screenshots and properties. It is a library (`ksv_driver_core`), so `tests/integration/driver_ui_test.cpp` drives it in-process.
- `driver_server.{h,cpp}`: the TCP transport.
- `driver_dataset.{h,cpp}`: session seeding. It reuses `tools/gallery/gallery_dataset.*`.
- `driver_main.cpp`: the host. It redirects settings, applies the headless look, loads Main through `gallery::QmlSourceInterceptor`, and writes `session.json`.
