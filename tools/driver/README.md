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
python scripts/ui_driver.py tour smoke --report  # every dataset's main screens, as a contact sheet
python scripts/ui_driver.py stop
```

Read the printed PNG path to see the UI. (`uv run python ...` works the same where uv is installed.)

## Sessions

A session lives in `.temp/ui-driver/<name>/` (default name `default`, pick another with `--session NAME`):

| Path | Contents |
|---|---|
| `data/` | the KovaaK's dir the app scans (`FPSAimTrainer/…`), `profile.pb`, `benchmarks/` |
| `settings/` | every `QSettings` (SettingsService and QML `Settings {}`); nothing touches the real registry or `%APPDATA%` |
| `shots/` | screenshots, numbered, plus `index.jsonl` (one line per shot) |
| `app.log` | the app's stdout/stderr, including Qt log messages |
| `launch.json` | the `start` options, reused by `restart` |
| `session.json` | pid, port and active injections, written once the app is ready |

- `start` wipes the session and seeds it (`--keep` reuses it); it stops a running session of the same name first.
- `restart` keeps data and settings and reuses the last `start` options. QML is loaded from `src/ui/qml` on disk, so **QML edits only need `restart`**; C++ edits need `restart --build`.
- A running session holds `build-agent/ksv_driver.exe` open, so stop it before a full `build_and_test.py` run (`restart --build` handles this itself).
- No session reads anything outside its directory: with no KovaaK's folder configured the app would fall back to the Steam install path, so the driver points that fallback at the nonexistent `<session>/no-kovaaks-install`.

`--dataset`:

| Name | Data |
|---|---|
| `rich` (default) | the gallery's synthetic profile (5 scenarios, a month of history, one with the long real name `Air CELESTIAL No UFO Easy Slowed`) plus the [benchmark fixtures](#benchmark-fixtures), opened on the demo benchmark |
| `single`, `short`, `bunched` | the gallery's other synthetic profiles (the primary scenario only, so the other benchmark rows are unplayed), plus the benchmark fixtures |
| `fixtures` | `tests/examples/*.perf` / `*.csv`, imported through the real ingestion pipeline |
| `empty` | an empty KovaaK's dir: no runs, no benchmarks |
| `first-run` | no KovaaK's dir configured, so the first-run welcome and banner show |

### Benchmark fixtures

The synthetic datasets copy every file in [`benchmarks/`](benchmarks) into the session and remember `driver-demo` as the selected benchmark (`--setting benchmarks/lastSelectedId=…` picks another). Their thresholds are fixed numbers picked against the rich profile's scores, so each row shows a different state; `DriverBenchmarkFixtureTest` (`tests/integration/driver_session_test.cpp`) fails when the run generator drifts from them.

| File | Against `rich` |
|---|---|
| `driver-demo.json` (5 tiers) | Clicking split into Static and Dynamic subcategories, Tracking flat. Rows: above Platinum (recent average back in the Platinum chunk), just into Bronze with the recent average still in Iron (the only Silver blocker), Gold nine tenths of the way to Platinum, Iron with the recent average below it, and played but below Iron. Summary: attained Bronze, none completed, next Silver; the rank history climbs in steps. |
| `driver-many-tiers.json` (12 tiers, Iron…Celestial) | Every row ranked, Iron to Nova: attained Grandmaster, completed Iron (the average-rank tick shows), next Nova with two blockers. The ladder overflows the grid at 960x700. |

Under `single`, `short` and `bunched` only the 1wall6targets row has runs; the others show as unplayed.

Other `start` options (all of them stored in `launch.json`, so `restart` reuses them):

| Option | Example | What it does |
|---|---|---|
| `--overlay DIR` (repeatable) | `--overlay tools/driver/tours/unreachable/broken` | Copies DIR's tree onto the session's `data/` after seeding, in order, replacing files. Lay it out like `data/`: `benchmarks/`, `FPSAimTrainer/performances/`, `FPSAimTrainer/stats/`, `profile.pb`. |
| `--setting KEY=VALUE` (repeatable) | `--setting file/kovaaksDirs=` | Writes any QSettings key before the app reads settings. VALUE is JSON when it parses (`false`, `3`, `["a","b"]`), otherwise a string; `KEY=` removes the key. `file/kovaaksDirs`, `file/kovaaks` and `file/profilePath` are written in SettingsService's own URL encoding. Keys: `file/…`, `benchmarks/lastSelectedId`, `graph/…`, and the QML `Settings {}` categories `window/`, `view/`, `graphAxis/`, `historyGraph/`, `graphSeriesVisibility/` (`view/playtimeGraphVisible=false`). |
| `--env KEY=VALUE` (repeatable) | `--env QT_SCALE_FACTOR=1.5` | Environment for `ksv_driver`, applied over the headless defaults; `KEY=` unsets. |
| `--profile-spec FILE` | `--profile-spec tools/driver/specs/many-scenarios.json` | Builds `profile.pb` (and optionally a benchmark) from a JSON spec; see [Profile specs](#profile-specs). Combines with `--dataset empty` (the default with a spec), `first-run` or `fixtures`. |
| `--inject PORT[.OP]=EFFECT[,EFFECT…]` (repeatable) | `--inject benchmarkStore.write=fail` | Fault and latency injection on the services `App` is built from; see [Fault injection](#fault-injection). |
| `--visible`, `--size WxH`, `--compiled-qml`, `--timeout S`, `--build`, `--keep` | `--size 960x700` | A real native window; the main window size; the qrc QML module instead of `src/ui/qml`; readiness timeout; build first; keep the session's data. |

Overlays, the driver's own settings and `--setting` apply only when a session is freshly seeded. `restart` (and `start --keep`) never re-applies them over the session's current state; `start --keep --overlay …/--setting …` applies the ones passed explicitly. `--env` and `--inject` apply on every launch.

**Headless rendering.** The headless run uses the `offscreen` platform with the software renderer, which is the same setup the QML tests use. The offscreen platform has no fonts, ignores the dark colour scheme and turns hover effects off, so `ksv_driver` points it at the Windows fonts, uses Segoe UI 9pt, turns hover effects on, and writes the Windows dark palette into every window (`driver_main.cpp` holds the snapshot of that palette). The result matches the real app closely. Use `--visible` when exact native rendering matters.

## Commands

Every command accepts `--session NAME` and `--json` (raw protocol result). Actions (`click`, `hover`, `mouse`, `drag`, `scroll`, `key`, `type`, `set`, `invoke`, `eval`, `resize`) also accept:
- `--shot`, to take a screenshot after the action; `--shot=LABEL` names it `shots/NNN-LABEL.png`, and `--shot-dir SUBDIR` puts it under `shots/SUBDIR/`;
- `--settle MS`, the wait after the action (default 120).

Any warnings the app logs during a command are printed as `app warning: …`; `start` also prints warnings logged while the app loaded (`app warning (startup): …`). Pointer actions report the target as it was resolved (the geometry the input used, its state before the action) and, when that changed, an `after:` line with its state now; a click on a menu item that closes its menu shows `after: … [hidden]`.

| Command | What it does |
|---|---|
| `windows` | Top-level windows. Settings and Benchmark Manager are separate windows (`settingsDialog`, `benchmarkManagerDialog`). A dialog's own window is keyed by the dialog's objectName (`dirtyClosePrompt`), else its id, else a slug of its title, and shows its kind: `fallback` (the QML implementation of a QtQuick.Dialogs dialog, headless; drive it like any window), `popup`, or for a dialog with no window of its own `native` (visible sessions; not capturable). |
| `shot [--window KEY \| --item SEL] [--name LABEL] [--dir SUBDIR] [--pad N] [-o PATH] [--all-windows] [--with-context]` | Saves a PNG and prints its path. `--item` crops to one item. `--name`/`--dir` name and group it (`shot --name dirty-close --dir manager`). `--with-context` paints a dialog window over the window it belongs to, at its position (`shot --window dirtyClosePrompt --with-context`). It warns when a visible dialog is not in the image. Every shot under `shots/` is recorded in `shots/index.jsonl`. |
| `tree [SEL] [--depth N] [--all] [--raw]` | Item tree. By default only named, id'd, texted or interactive items are shown. `--raw` shows every item; `--all` includes hidden items. |
| `find SEL [--all] [--limit N] [--objects]` | Every match, with type, name, id, text and `@x,y wxh` window coordinates. `--objects` lists only non-visual QML objects: `find --objects "obj:*Dialog"`. |
| `click SEL` / `click --at X Y`, `dblclick`, `rclick` | Real synthesized mouse events at the item's center. `--offset DX DY` makes the point relative to the item's top-left; `--mods ctrl+shift` adds modifiers. |
| `hover SEL [--steps N]` | Moves the cursor from its last position to the item through N moves (default 8), as a real pointer does, so hover-delayed behaviour fires: `hover "text:Scenario Graph Lines" --settle 400` opens that submenu. |
| `mouse press\|move\|release [SEL \| --at X Y] [--button B] [--mods M] [--steps N]` | One stage of a gesture that spans commands: the driver keeps the cursor position and held buttons per window, so you can screenshot mid-drag. `move` interpolates N moves (default 12). `release` with no target releases where the cursor is. `stop` and the end of every batch release held buttons. `mouse press seriesDragHandle_1`, `mouse move seriesDragHandle_3`, `shot`, `mouse release`. |
| `drag FROM TO` (selector or `X,Y`), `scroll SEL --dy N` | `drag` is press, move, release in one command. Positive `--dy` scrolls down, one wheel notch per unit. |
| `key SEQ [--repeat N]` | Keys such as `Return`, `Escape`, `Tab`, `Down`, `Ctrl+A`, `F2`. They go to the focused window. |
| `type TEXT [--into SEL] [--clear] [--at-start] [--enter]` | Types into the focused item. `--into` focuses the item and puts the caret at the end, so text is appended (`type " (x)" --into seriesNameField_2` gives `Accuracy (x)`); `--clear` replaces the text; `--at-start` types at the start. |
| `get SEL PROP`, `set SEL PROP VALUE` | Reads or writes a QML property (`text`, `currentIndex`, `Layout.fillWidth`, …). `VALUE` is parsed as JSON when it can be, otherwise taken as a string. |
| `invoke SEL METHOD [ARGS…]` | Calls a method, slot or QML function, or emits a signal (`open`, `close`, `clicked`, `trigger`): `invoke obj:manageBenchmarksAction trigger`. |
| `eval EXPR [--on SEL]` | A QML/JS expression. It is evaluated in `Main.qml`'s scope, so `root`, `settingsDialog`, `workspaceTabBar` and the view models (`sessionVm`, `benchmarkManagerVm`, …) are available; `--on` uses an item's scope instead. |
| `wait [SEL] [--prop P --equals V] [--expr E] [--gone] [--timeout MS]` | Polls until the condition holds. |
| `resize WxH [--window KEY]`, `logs [--all] [--warnings] [--clear]` | |
| `put SRC… [--to FOLDER]` | Copies files into the session's `data/` while it runs: `--to performances\|stats\|benchmarks\|.` or a path under `data/`; by default `.perf` goes to performances, `.csv` to stats, `.json` to benchmarks. `put "tests/examples/1wall6targets TE.perf"` imports a run live (the performances folder is watched). |
| `rm PATH…` | Deletes files under `data/` (`rm benchmarks/driver-demo.json`); refuses anything outside it. |
| `edit PATH --replace OLD NEW … \| --from SRC` | Rewrites a file under `data/` in place; OLD must occur. `edit benchmarks/driver-demo.json --replace "Driver Demo" "Renamed"`. |
| `batch FILE\|- [--keep-going] [--fail-on-warnings] [--var NAME=VALUE]` | Runs a step per line; see [Batches and tours](#batches-and-tours). |
| `tour [NAME] [--report] [--stop-on-failure] [--fail-on-warnings] [--keep-running]` | Runs `tools/driver/tours/NAME.txt` in session `tour-NAME`; without NAME, lists the tours. `tour unreachable --report`. |
| `report [DIR] [-o FILE] [--coverage FILE] [--title T] [--link]` | Builds a contact sheet (`index.html`) from a shots directory (default: the session's `shots/`). `report --session ses`. |
| `start`, `restart`, `stop`, `status` | Session lifecycle. `status` also lists the session's spec, overlays, settings, env and injections. |

`put`, `rm` and `edit` then wait until the app has processed the change: no profile build in progress and no new log entries for 300 ms, capped by `--timeout` (default 5 s; it reports how long it waited). Only the performances folder is watched, so stats and benchmark changes need a refresh: the Manager's `refreshLibraryButton`, or `eval benchmarkManagerVm.refresh()`.

## Selectors

```
[win:<window>/] segment ( " > " segment )*
segment := term ( "&" term )* [ "@" N ]
term    := <objectName glob> | name:<glob> | id:<glob> | text:<glob> | rawtext:<glob>
         | type:<glob> | acc:<glob> | obj:<glob>
```

- A bare word matches `objectName`. Globs use `*` and `?`, so `scenarioItem_*` works.
- `id:` matches a QML id from the app's own `.qml` files.
- `text:` matches `text`, `title` or `placeholderText`. `acc:` matches `Accessible.name`. Both are case-insensitive and ignore mnemonic markers: `text:File` matches the `&File` menu, and `&&` stands for a literal `&`. `rawtext:` matches the raw string (`rawtext:\&File`).
- `type:` matches the QML type or any base type: `type:Button`, `type:AbstractButton`, `type:ScenarioWorkspace`.
- `obj:` matches non-visual QML objects (Action, Timer, Settings, MessageDialog/FileDialog/FolderDialog/ColorDialog) by objectName or id. They have no geometry, so `get`, `set` and `invoke` work on them and input commands refuse them. `obj:` selectors also search hidden places, such as the Actions of a closed menu.
- `a > b` finds `b` among `a`'s descendants. `@N` picks the Nth match of a segment (0-based; `@-1` is the last).
- `win:settingsDialog/…` searches only that window. Window keys come from `windows`; the main window is `main`.
- Inside a term, `\` escapes `& > @ : " \`, and `"…"` quotes a literal run: `text:"Save & close"`, `text:\&File`, `name\:with\:colons`. An unescaped `&` is always the AND operator, so `text:&View` is an error.

Matching rules:
- Hidden items, such as other `StackLayout` pages or closed dialogs, are skipped unless you pass `--all`.
- When one target is needed and both a control and its own label match (`text:Close`), the control wins.
- Otherwise an ambiguous selector fails and lists its candidates.
- A selector that matches nothing reports similar names and how many hidden items would match.

Prefer `objectName`s. `Main.qml` and the workspaces have many: `scenariosTab`, `benchmarksTab`, `workspaceTabBar`, `scenarioSearchField`, `manageBenchmarksButton`, `benchmarkSelector`, … Adding an `objectName` to a QML item you want to drive is cheap.

## Profile specs

`--profile-spec FILE.json` builds the session's profile from a declarative spec instead of a dataset (`tools/driver/driver_spec.*`; example: `tools/driver/specs/many-scenarios.json`). Every field but a scenario's `name` has a default; unknown fields and bad values fail before the app starts, naming the field (`scenarios[3].runsPerDay: expected an integer >= 1`).

```json
{
  "scenarios": [
    {"name": "Pasu Spec", "hash": "spec-pasu", "runs": 30, "firstDay": 0, "daySpacing": 1, "runsPerDay": 3,
     "startMinute": 1080, "minuteSpacing": 2, "durationSeconds": 60, "performance": true, "shape": "gallery-pasu"}
  ],
  "benchmark": {"id": "spec-benchmark", "name": "Spec Benchmark", "file": "spec-benchmark.json",
                "tierFactors": [0.6, 0.8, 0.95, 1.05, 1.2],
                "categories": [{"name": "Clicking", "color": "#00bcd4", "scenarios": ["Pasu Spec", {"name": "X", "hash": "x"}]}]}
}
```

| Scenario field | Default | Meaning |
|---|---|---|
| `name` | required | display name; several scenarios may share one |
| `hash` | `spec-<slug of name>` | scenario identity |
| `runs` | 10 | number of runs |
| `firstDay`, `daySpacing` | 0, 1 | day of the first run (day 0 is 2026-08-01) and days between run days |
| `runsPerDay`, `startMinute`, `minuteSpacing` | 1, 1080, 2 | runs per day, minute of day of the first (1080 = 18:00), minutes between them |
| `durationSeconds` | 60 | run length |
| `performance` | true | false keeps only the run totals, as a stats-CSV-only import does (the run graph shows no performance data) |
| `shape` | `gallery-primary` | a gallery shape (`gallery-primary`, `gallery-pasu`, `gallery-smoothbot`, `gallery-flyts`, `gallery-celestial`) or base values: `{"shotsPerSecond", "shotVariation", "accuracy", "hitsPerKill", "damagePerHit", "scorePerHit", "scorePerKill"}` |

`benchmark` is either a path to a benchmark JSON file to copy (relative to the spec) or an object: `id`, `name`, `file`, then `tiers` (`[{"name", "id", "factor", "color"}]`) or `tierFactors` (up to five factors for Iron…Platinum; default `[0.6, 0.8, 0.95, 1.05, 1.2]`), and `categories` (`[{"name", "id", "color", "scenarios"}]`; default one category with every scenario). A category names scenarios by name or by `{"name", "hash"}`. Thresholds are each tier's factor times the scenario's personal best; a name that matches no scenario, or several, is written without a hash (unresolved or ambiguous) around a nominal 1000.

Generation is deterministic: runs are seeded from their scenario and run index, scenarios are generated in order, and the store's creation time is pinned, so a spec always writes the same bytes. The four synthetic datasets use the same run generator (`gallery::makeRun`).

## Fault injection

`--inject PORT[.OP]=EFFECT[,EFFECT…]` wraps the services the driver hands to `App`'s injecting constructor in decorators (`tools/driver/driver_ports.*`). Omitting OP applies the effects to every op of the port. Each injection is logged in `app.log` (`ksv_driver: inject benchmarkStore.write=fail`) and listed by `status`.

| Port | Ops | A failed call returns | Runs on |
|---|---|---|---|
| `decoder` | `decode`, `decode_file` | a thrown `std::runtime_error`, as a bad `.perf` does | `decode_file`: `SessionController`'s worker thread during profile builds (startup, Rebuild Profile); the GUI thread for live imports and File > Load Performance File. `decode` is not called through the port. |
| `csv` | `parseFile` | `std::nullopt` | as `decode_file` |
| `benchmarkStore` | `scan`, `write`, `remove` | `DirectoryUnavailable` / `WriteFailed` | GUI thread |
| `seriesStore` | `load`, `save` (the graph-configuration document, Graph Lines included) | `load`: the default configuration, leaving the stored document untouched; `save`: nothing is written and nothing reports it, since writes are never verified | GUI thread (`load` once, while `App` is built) |

| Effect | Meaning |
|---|---|
| `delay:MS` | sleep on the calling thread, then delegate. On a GUI-thread port this freezes the UI for that long. |
| `fail` | every call fails |
| `fail:N` | only the Nth call (1-based) fails |
| `fail:after:N` | every call after the Nth fails (`benchmarkStore.scan=fail:after:1` keeps the startup scan and fails every refresh) |

```
start --dataset fixtures --inject decoder=delay:300                 # Rebuild Profile shows its progress bar
start --inject benchmarkStore.write=fail                            # Manager saves fail
start --inject seriesStore.save=fail                                # graph settings saves are silently lost
```

## Batches and tours

`batch FILE` runs one step per line: a CLI line (`click benchmarksTab --shot`) or a raw JSON request (`{"cmd": "find", "args": {...}}`). `#` lines are comments. Besides commands, a batch understands:

| Line | Meaning |
|---|---|
| `set NAME=value` | a variable; `${NAME}` is replaced in later lines (`set M=win:benchmarkManagerDialog`, then `click "${M}/saveButton"`). `${BATCH_DIR}` (the batch file's folder), `${REPO}` and `${SESSION}` are predefined; a tour adds `${TOUR_DATA}` (`tools/driver/tours/NAME/`). An unknown `${NAME}` fails the step. |
| `section NAME` | the default `shots/` subdirectory for later shots (and the report's group) |
| `options --keep-going --fail-on-warnings` | the batch flags, from inside the file |
| `start …`, `restart`, `stop` | lifecycle steps on the batch's own session; a batch can start several sessions in turn |

A failing step stops the batch unless `--keep-going`. Either way the batch ends with a summary of its steps and failures and exits 1 if any step failed. `--fail-on-warnings` counts a step during which the app logged a warning (at startup too) as failed.

`tour NAME` runs `tools/driver/tours/NAME.txt` in session `tour-NAME`, keeps going past failures (`--stop-on-failure` to stop), stops the session at the end (`--keep-running` to keep it), and collects everything in `.temp/ui-driver/tours/NAME/`: `shots/`, `summary.json` and, with `--report`, `index.html`. A tour's data files sit in `tools/driver/tours/NAME/`, and an optional `coverage.json` there (`[{"area", "state", "shot"}]`, where `shot` is a shot label) becomes the report's coverage table, each state marked captured or missing.

| Tour | Covers |
|---|---|
| `smoke` | the main screens of every dataset at 1280x800 and 960x700, failing on any app warning; about 20 s. Run it before committing UI changes; it is not wired into CTest. |
| `full` | every state of the 2026-10-11 sweep, with named shots |
| `unreachable` | every state that sweep could not reach: data states, faults, held gestures and dialog prompts |
| `composing-example` | the worked example below |

`report` builds a self-contained page (images embedded; `--link` links them instead): a summary with failed steps, the coverage table, then the shots grouped by subdirectory with their labels, the command that took them and any app warnings. Click a shot to enlarge it. Without an `index.jsonl` it lists the numbered PNGs.

## Composing a session

A session is built in layers, each over the previous one:

1. **dataset**: `--dataset` seeds `data/` (`rich`, `fixtures`, `empty`, …);
2. **profile spec**: `--profile-spec` replaces `profile.pb` (and can add a benchmark);
3. **overlays**: each `--overlay DIR`, in order, copied onto `data/`;
4. **driver settings**: the session's KovaaK's folder and profile path (and the sandboxed install fallback);
5. **settings**: each `--setting KEY=VALUE`, in order, so they can undo the driver's;
6. **env**: `--env` for the `ksv_driver` process;
7. **injections**: `--inject` decorators around the services `App` is built from.

Then, while it runs: `put`/`rm`/`edit` change data on disk, and `mouse`, `obj:` selectors and dialog window keys reach states that need a held gesture, a non-visual object or a prompt.

A niche state should need a tour and some data files, not driver code. For example, benchmark rows whose scenario is missing (unresolved) or shared by two scenarios (ambiguous) exist in no dataset. [`tours/composing-example.txt`](tours/composing-example.txt) reaches them:

1. **Spec**: [`composing-example/twins.json`](tours/composing-example/twins.json) makes a profile with `Solo Scenario` and two `Twin Scenario`s that have different hashes.
2. **Overlay**: [`composing-example/overlay/benchmarks/matching.json`](tours/composing-example/overlay/benchmarks/matching.json) is a benchmark naming Solo (with its hash), Twin (no hash: two candidates) and `Ghost Scenario` (no hash, no such scenario).
3. **Tour**: one `start` line composes them, and `--setting benchmarks/lastSelectedId=matching-demo` opens the Benchmarks tab on that benchmark:
   ```
   start --profile-spec "${TOUR_DATA}/twins.json" --overlay "${TOUR_DATA}/overlay" --setting benchmarks/lastSelectedId=matching-demo
   set M=win:benchmarkManagerDialog
   section benchmarks
   click benchmarksTab --shot=matching-rows
   section manager
   invoke obj:manageBenchmarksAction trigger
   dblclick "${M}/libraryEntry_0"
   click "${M}/cell_entry-twin_scenario"
   shot --window benchmarkManagerDialog --name ambiguous-row-mapping
   ```
4. **Report**: `ui_driver.py tour composing-example --report` prints `.temp/ui-driver/tours/composing-example/index.html`: the tracking view with "Several scenarios match" and "Not found in your runs", and the Manager's mapping panel listing both Twin candidates. Its `coverage.json` marks the three states captured.

## Protocol

`ksv_driver --session-dir DIR [--dataset NAME] [--size WxH] [--compiled-qml] [--overlay DIR]… [--setting K=V]… [--reapply] [--profile-spec FILE] [--inject SPEC]…` listens on a random `127.0.0.1` port, which it records in `DIR/session.json`. Requests and responses are newline-delimited JSON:

```
{"id": 1, "cmd": "click", "args": {"selector": "benchmarksTab"}}
{"id": 1, "ok": true, "result": {"target": {...}, "after": {...}, "pos": [960, 39]}}
{"id": 2, "ok": false, "error": "selector 'type:TabButton' is ambiguous: …", "details": {"candidates": [...]}}
```

The commands are the CLI's, under their protocol names: `ping`, `windows`, `tree`, `find`, `screenshot`, `click`, `hover`, `mouse`, `drag`, `scroll`, `key`, `type`, `get`, `set`, `invoke`, `eval`, `wait`, `resize`, `logs` and `quit`. `UiDriver::dispatch` in `ui_driver.cpp` lists every argument. The CLI client lives in `scripts/ui_automation/`.

## Layout

- `ui_driver.{h,cpp}`: scene walk, selectors, input, screenshots and properties. It is a library (`ksv_driver_core`), so `tests/integration/driver_ui_test.cpp` drives it in-process.
- `selector.{h,cpp}`: the selector parser.
- `driver_server.{h,cpp}`: the TCP transport.
- `driver_dataset.{h,cpp}`: session seeding. It reuses `tools/gallery/gallery_dataset.*`.
- `driver_spec.{h,cpp}`: profile specs and the benchmark generator; `driver_ports.{h,cpp}`: the `--inject` decorators; `session_compose.{h,cpp}`: overlays and `--setting` (tested in `tests/integration/driver_session_test.cpp`).
- `driver_main.cpp`: the host. It redirects settings, composes the session, applies the headless look, loads Main through `gallery::QmlSourceInterceptor`, and writes `session.json`.
- `benchmarks/`: the benchmark fixtures the synthetic datasets ship with. `specs/`: example profile specs. `tours/`: checked-in tours and their data.
- `scripts/ui_automation/`: the CLI (`cli.py`), sessions (`session.py`), the client, result formatting and the contact sheet (`report.py`).
