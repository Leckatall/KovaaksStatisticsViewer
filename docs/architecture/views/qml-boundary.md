---
id: qml-boundary
name: QML boundary
viewpoints: [structural, information, runtime]
---

# QML boundary

## Concern and reader outcome

QML assembles windows from C++ view models and persists its own visual settings. Use this view when exposing a C++ view model, adding a QML panel or changing persisted visual settings. It explains property injection, type registration, interaction patterns and the authority split between QML and `ISettingsService`.

## Scope and exclusions

This view covers the nine view models handed to `Main.qml` and what QML does with them. Related views own the following details: the construction and ownership of the view models in C++ ([Composition root][composition-root]), the settings-visit draft protocol ([Graph configuration][graph-configuration]), benchmark-specific QML constraints ([Benchmark editing][benchmark-editing]), capture dialogs ([Image capture][image-capture]), the graph canvas and its models ([Graph presentation][graph-presentation]), and selection fan-out ([Run selection and rendering][run-selection-and-rendering]).

## Architectural account

### How view models reach QML

`App::start()` calls `setInitialProperties(initialProperties())` on the engine and then `loadFromModule("KovaaksStatsViewer", "Main")`; `initialProperties()` is the single list of what QML receives ([`App::initialProperties`](../../../src/app/app.cpp)). `Main.qml` declares each as `required property var`: `graphVm`, `playtimeVm`, `historyVm`, `sessionVm`, `settingsVm`, `scenarioBrowserVm`, `benchmarkManagerVm`, `benchmarkTrackingVm`, `captureVm` ([`Main.qml`](../../../src/ui/qml/Main.qml)). A missing map entry leaves a required property unset and prevents `Main.qml` from loading. Removing the QML declaration leaves the map entry without a destination; remaining QML references to that property also fail. `start()` itself checks only that a root object exists and returns -1 otherwise ([`App::start`](../../../src/app/app.cpp)).

The properties are typed `var`, so QML member access on a view model is resolved at run time and qmllint cannot check it against the C++ types. A mistyped binding surfaces only when the real view model is loaded; [`tests/integration/qml_smoke_test.cpp`](../../../tests/integration/qml_smoke_test.cpp) states that it boots the real `App` and QML module for this reason.

### How QML properties are typed

QML properties that hold a C++ object or a plain value are typed where a concrete type exists, so that qmllint and qmlls can check member access through them. View models and local component handles need separate explanation:

- **View models stay `var`.** The Qt Quick tests pass JavaScript objects or `QtObject` fakes for every view model, and `ScenarioHistoryPanel` passes local `QtObject` adapters as `graphVm` and `visualSettings` in production too. A typed property would reject both. The one exception is the graph view model on the canvas wrapper, typed `GraphViewModelBase` ([`GraphCanvasWithTooltip.qml`](../../../src/ui/qml/graph/GraphCanvasWithTooltip.qml)).
- **Local component handles are sometimes typed with the application's own QML components.** `BenchmarkEditorTable.activeEditor` is a `BenchmarkCellEditor` and `SettingsDialog.focusedField` is a `CommitOnFinishField`, cast from the focused item ([`BenchmarkEditorTable.qml`](../../../src/ui/qml/benchmark/BenchmarkEditorTable.qml), [`SettingsDialog.qml`](../../../src/ui/qml/shell/SettingsDialog.qml)). Caution is warranted because the QML tests import component directories as `file:///` URLs while `import KovaaksStatsViewer` loads the same files from `qrc:/`, so one component name could resolve to two incompatible types depending on the importer. That is an unverified constraint: it comes from the changelog fragment below, no code comment or test states it, and the two typed handles show the source does not follow it universally.

The `file:///` versus `qrc:/` concern is recorded in the changelog fragment that introduced the typing (`changelog.d/2026-11-24-qml-typed-properties.md`), not in a code comment.

QML uses registered C++ types both for property declarations and for named instantiation, so both depend on type registration. `BenchmarkEditorTable.tableModel` is typed `BenchmarkTableModel` ([`BenchmarkEditorTable.qml`](../../../src/ui/qml/benchmark/BenchmarkEditorTable.qml)) and `GraphCanvasWithTooltip.graphVm` is typed `GraphViewModelBase`; the expression editor types its nodes `EditableExpressionNode`. Dropping the registration of any of these leaves the typed property naming an unknown type; the failure mode at load time was not exercised.

The menu bar, status bar, two workspaces and three application windows directly configured by `Main.qml` receive these properties; nested panels and file/message dialogs are excluded from this table.

| Surface | Injected view models | Visual settings |
|---|---|---|
| `AppMenuBar` | `graphVm`, `historyVm` | `visualSettings` |
| `ProfileStatusBar` | `sessionVm`, `captureVm` | — |
| `ScenarioWorkspace` | `graphVm`, `playtimeVm`, `historyVm`, `scenarioBrowserVm`, `settingsVm`, `captureVm` | `visualSettings` |
| `BenchmarkWorkspace` | `benchmarkTrackingVm`, `settingsVm`, `captureVm` | `visualSettings` |
| `SettingsDialog` | `settingsVm`, `sessionVm` | `visualSettings` |
| `CapturePreviewDialog` | `captureVm` | — |
| `BenchmarkManagerDialog` | `benchmarkManagerVm` | — |

Only `Main.qml` holds all nine; each surface receives the subset it needs, and the workspaces forward subsets to their panels. The manager window alone receives `benchmarkManagerVm` directly from `Main.qml` and forwards it to its table (`manager: root.benchmarkManagerVm` in [`BenchmarkManagerDialog.qml`](../../../src/ui/qml/benchmark/BenchmarkManagerDialog.qml); see [Benchmark editing][benchmark-editing]).

### What registration requires

`ksv_ui` is a static library built with `qt_add_qml_module(... NO_PLUGIN)` ([`src/ui/CMakeLists.txt`](../../../src/ui/CMakeLists.txt)), and the repository's rule is that a static module's auto-registration objects are dropped by the linker when nothing references them (`AGENTS.md`; the comment on `declare_metatypes()`). Types are therefore registered by two mechanisms that coexist in the code:

| Mechanism | Types in the reviewed headers |
|---|---|
| `QML_ELEMENT` and an explicit `qmlRegister…` call (two routes) | `GraphViewModelBase`, `GraphViewModel`, `PlaytimeGraphViewModel`, `BenchmarkManagerViewModel`, `BenchmarkTableModel`, `CaptureViewModel`, `BenchmarkHistoryViewModel`, `GraphCanvas`, `CapturePreviewItem`, the `Editable…Node` family |
| Explicit call only | `CompletionHistoryViewModel`, `BenchmarkTrackingViewModel`, `BenchmarkProgressGridModel` |
| `QML_ELEMENT` only | `ScenarioBrowserViewModel` (`App` references it, so its registration is linked) |
| Neither | `SessionViewModel`, `SettingsViewModel` (reached only as untyped QObjects through `var`) |

Evidence: [`src/qml_registration.h`](../../../src/qml_registration.h), and the `QML_ELEMENT` macros under [`src/ui/presentation`](../../../src/ui/presentation) and [`src/ui/components`](../../../src/ui/components). Registration matters where QML names the type: `GraphCanvas { … }` is instantiated, and `GraphViewModel.HasData` is referenced as an enum in [`DashboardGraphCanvas.qml`](../../../src/ui/qml/graph/DashboardGraphCanvas.qml). The session and settings view models need no named QML type registration because QML receives them as untyped object properties. Whether a type with both a macro and an explicit call is registered once or twice was not examined.

Three requirements follow:

- **`declare_metatypes()` must run before any engine loads the module, in every executable that does so.**
- **Add each directory containing a `QML_ELEMENT` header to the include path.** [`src/ui/CMakeLists.txt`](../../../src/ui/CMakeLists.txt) lists these directories. Its comment states that `qmltyperegistrar` includes each header by bare filename, guarded by `__has_include`; a header outside the include path therefore has its type silently omitted. A header in a new directory requires a new include-path entry. The generated registrar code was not inspected for this view.
- **Types in QML-facing meta signatures must resolve without `declare_metatypes()`.** [`tests/ui/qml_signature_metatypes_test.cpp`](../../../tests/ui/qml_signature_metatypes_test.cpp) records that when moc sees only a forward declaration, the type is resolved at call time and depends on an unrelated registration being present. The test runs in a separate executable so that `declare_metatypes()` cannot mask that dependency. A QML-invokable method that returns or accepts a forward-declared type is fragile for this reason.

The six call sites are [`src/main.cpp`](../../../src/main.cpp), [`tools/gallery/gallery_main.cpp`](../../../tools/gallery/gallery_main.cpp), [`tools/driver/driver_main.cpp`](../../../tools/driver/driver_main.cpp), [`tests/integration/qt_test_main.cpp`](../../../tests/integration/qt_test_main.cpp), [`tests/ui/qml/tst_ui_qml.cpp`](../../../tests/ui/qml/tst_ui_qml.cpp) and [`tests/ui/benchmarks/manager/benchmark_table_contract_test.cpp`](../../../tests/ui/benchmarks/manager/benchmark_table_contract_test.cpp). In `main()` it is the second statement, before the `QGuiApplication` is constructed.

To inject a new view model, construct it in `App`, add it to `initialProperties()` and declare a matching `required property var` in `Main.qml`. Check named-type registration separately when QML refers to its type.

### Interaction patterns

A panel receives what it needs by property assignment from its parent.

| Pattern | Where | Consequence |
|---|---|---|
| Required view model property, assigned down the tree | `Main.qml` to workspaces to panels (`graphVm`, `historyVm`, `scenarioBrowserVm`, `trackingVm`). | The panel talks to the view model directly; a panel test must supply a real or fake object with the same members. |
| Optional view model, default `null` | `settingsVm` and `captureVm` on graph panels, `GraphGear`, `CapturePreviewDialog`. | `GraphGear` opens its popup with defaults, and `CapturePreviewDialog` guards access when `captureVm` is null, so QML tests can omit the view model. |
| Signal-out panel, no view model | `SelectionPanel` emits `searchEdited`, `scenarioActivated`, `runSelected`, `sortRequested` and `scenarioSortRequested` ([`SelectionPanel.qml`](../../../src/ui/qml/scenario/SelectionPanel.qml)). `ScenarioWorkspace` translates each into a call on `scenarioBrowserVm`. | The panel holds no view model reference ([`ScenarioWorkspace.qml`](../../../src/ui/qml/scenario/ScenarioWorkspace.qml)); the owner decides what the intent means. |
| Requests that need a window go up to `Main.qml` | `configureGraphLinesRequested`, `moreGraphSettingsRequested(graphKey)`, `manageBenchmarksRequested`, `editBenchmarkRequested(filename, benchmarkId)`. | Only `Main.qml` holds the windows. |
| Leaf commits straight to the view model | `GraphGear` calls `settingsVm.applyQuickPatch(graphKey, patch)`; the perf file dialog calls `graphVm.fetchData(url)`. | No draft in QML: `GraphGear` submits each edit directly to the quick-patch path, which commits configuration (see [Graph configuration][graph-configuration]). |
| C++ to QML through `Connections` | `Main.qml` listens to `captureVm.previewClosed` and `saveDestinationRequested`, and to `graphVm.seriesConfigurationChanged`. | The view model announces; `Main.qml` reacts at window level. |
| QML hands live objects to C++ | The canvas is passed to `captureVm.quickCopy(key, canvas)`; `Main.qml` calls `captureVm.useTheme(Theme, font)`; `GraphCanvas.graphVm` takes a view model reference. | C++ reads QML-side objects at call time; the property names form a runtime contract. |
| Window visibility | The capture preview is bound to `captureVm.previewOpen`; the settings and manager windows are opened by QML calling `open()`. | The capture window is view-model driven; the others are QML driven. |

`Main.qml` re-exposes the manager window as the property alias `benchmarkManagerDialog` because that window is outside its item tree ([`Main.qml`](../../../src/ui/qml/Main.qml)).

The capture preview refuses to close while `previewOpen` is true. Close vetoes are GUI-thread events; its source documents that a refusal also cancels `Qt.quit()` ([`CapturePreviewDialog.qml`](../../../src/ui/qml/shell/CapturePreviewDialog.qml); [Image capture][image-capture] owns the capture protocol).

`captureAppearanceFromTheme` reads `Theme.palette` roles and the `attention` and `groupTintAlpha` properties by name ([`capture_appearance.cpp`](../../../src/ui/capture/capture_appearance.cpp)). Renaming a `Theme` property breaks capture colours without a compile error.

The selection entry points and the graph-config settings-visit calls that these patterns carry are described in [Run selection and rendering][run-selection-and-rendering] and [Graph configuration][graph-configuration].

### QML-owned persistent state

`VisualSettingsManager` and `SettingsDialog` each use QtCore `Settings` blocks, which persist through `QSettings`. `main()` sets the organisation name, application name and the default `QSettings` format (INI) before constructing `App`, so in production those blocks resolve to the same file that [`SettingsService`](../../../src/qt_data/settings_service.cpp) addresses explicitly with `QSettings(format, UserScope, "Lecka", "KovaaksStatsViewer")` and an INI default format ([`main.cpp`](../../../src/main.cpp)). The two sides use disjoint key groups.

| State | Group and keys | Identity | Authority |
|---|---|---|---|
| Main window geometry | `window/width`, `height`, `x`, `y`, `hasPosition` | Fixed keys | QML only. Recorded only while `visibility === Window.Windowed`, so a minimised or maximised window does not overwrite the windowed geometry. |
| Settings dialog size and selected page/section | `window/settingsDialogWidth`, `settingsDialogHeight`, `settingsDialogPage`, `settingsDialogSection` | Fixed keys; the page and section are written with `setValue` and read with `value`, and `sync()` is called when the dialog hides | QML only. |
| Panel and graph visibility | `view/*`: ten named boolean properties, six of them one per graph (stored as `view/scenarioGraphVisible`, `playtimeGraphVisible`, `scenarioHistoryGraphVisible`, `benchmarkRankGraphVisible`, `benchmarkRecentRankGraphVisible`, `benchmarkPlaytimeGraphVisible`) | The storage keys are those property names; `VisualSettingsManager.graphVisibilityProperties` maps each graph key string (`run`, `dailyPlaytime`, `scenarioHistory`, `benchmarkRank`, `benchmarkRecentRank`, `benchmarkPlaytime`) to its persisted property name | QML only. C++ never reads or writes them. |
| History graph series | `historyGraph/columnVisibility` (map keyed `"1"` to `"6"`), `yAxisColumnKey` | The fixed history column numbers | QML only. |
| Run graph series visibility | `graphSeriesVisibility/visibleSeriesIds`, `seenSeriesIds` | Series ids as strings, the same ids the C++ run-graph configuration enables (`graphVm.enabledSeriesIds`) | Split: C++ decides which series exist and are enabled; QML decides which enabled ones are shown. |


The gallery is a separate setup ([`gallery_main.cpp`](../../../tools/gallery/gallery_main.cpp)): it redirects both stores to one temporary base directory but sets the application name `KovaaksStatsViewer-Gallery`, while the C++ adapter still selects the production name, so it does not demonstrate a shared file.

The quick-hide notice (`hideNoticeGraph`) lives outside the `Settings` blocks and is cleared by restart.

The C++ groups, by contrast, are `file/*`, `graph/*`, `graphAxis/*` and `benchmarks/*` ([`settings_service.cpp`](../../../src/qt_data/settings_service.cpp)); the `window` group is shared by two QML owners whose keys do not overlap. The graph configuration itself is versioned, validated and quarantined on the C++ side ([Graph configuration][graph-configuration]); the QML-owned state has no schema version and no migration. One explicit compatibility rule is [`isHistorySeriesVisible`](../../../src/ui/qml/graph/VisualSettingsManager.qml)'s default of shown for a missing `"6"` entry, with a comment that maps saved before the average existed lack it.

**Series-visibility reconciliation.** `syncVisibleSeriesIds(enabledIds)` ([`VisualSettingsManager.qml`](../../../src/ui/qml/graph/VisualSettingsManager.qml)) runs from [`Main.qml`](../../../src/ui/qml/Main.qml) on `Component.onCompleted` and on every `graphVm.seriesConfigurationChanged`. For each enabled id it keeps the stored visibility if the id was seen before and shows it if it is new; ids no longer enabled are dropped from `visibleSeriesIds`, and `seenSeriesIds` is replaced by the current enabled list. Consequence: a series that is disabled and later re-enabled has left `seenSeriesIds` in between, so it comes back shown regardless of how it was last toggled. The run graph then draws `enabledSeriesIds` filtered by `isSeriesVisible`, mapped to columns, in [`DashboardGraphCanvas.qml`](../../../src/ui/qml/graph/DashboardGraphCanvas.qml).

### Startup ordering

This sequence runs on the GUI thread: `main()`, `App::start()`, the engine's creation of `Main` and its `Component.onCompleted` handler are one thread of execution.

`Main.qml` binds its width and height to the stored window size and starts hidden. Its `Component.onCompleted` handler reconciles series visibility, passes `Theme` to `captureVm.useTheme`, restores the position and finally shows the window. The comment in source gives the reason: a window shown during creation is placed by the OS first, and that placement would be recorded over the saved position. `restorePosition()` applies the stored position only when `hasPosition` is set and `isVisibleOnScreens` finds enough of the window's top edge on a current screen, so an unplugged monitor does not strand the window.

### Theme propagation

`Theme` ([`Theme.qml`](../../../src/ui/qml/common/Theme.qml), declared a singleton in [`src/ui/CMakeLists.txt`](../../../src/ui/CMakeLists.txt) with `QT_QML_SINGLETON_TYPE`) supplies the application palette. The comment on it states that each top-level window must set `palette: Theme.palette`, because unset roles fall back to the Windows theme. In the source read, all four `ApplicationWindow`s do: [`Main`](../../../src/ui/qml/Main.qml), [`SettingsDialog`](../../../src/ui/qml/shell/SettingsDialog.qml), [`BenchmarkManagerDialog`](../../../src/ui/qml/benchmark/BenchmarkManagerDialog.qml) and [`CapturePreviewDialog`](../../../src/ui/qml/shell/CapturePreviewDialog.qml). A fifth window added without that line renders with system colours. `main()` sets the colour scheme to dark, which the comment says affects native title bars and file, message and colour dialogs rather than the app's own windows.

## Consequences and invariants

- **In production a view model reaches QML only through `initialProperties()` and `Main.qml`.** The gallery is a deliberate exception: it also injects real `App` models as root-context properties so live reload can recreate the root ([`gallery_main.cpp`](../../../tools/gallery/gallery_main.cpp)).
- **Registration is by hand where `QML_ELEMENT` alone is not enough.** A type has one or two routes: the `QML_ELEMENT` route needs its header directory in `src/ui/CMakeLists.txt`, and the explicit route needs its entry in `declare_metatypes()`. Removing the sole registration route makes a named type unavailable (for example, explicit-only `CompletionHistoryViewModel` or `QML_ELEMENT`-only `ScenarioBrowserViewModel`). `GraphViewModelBase` and `BenchmarkTableModel` have both routes; because typed properties name them, losing both breaks those properties. For a type with both routes, removing one leaves the other, so that change alone does not demonstrate whether registration is still required. Neither removal produces a compiler or linker diagnostic. The integration smoke test loads the real compiled module with the real `App` and `Main` tree ([`qml_smoke_test.cpp`](../../../tests/integration/qml_smoke_test.cpp)). A separate native contract test loads `BenchmarkEditorTable` from the module ([`benchmark_table_contract_test.cpp`](../../../tests/ui/benchmarks/manager/benchmark_table_contract_test.cpp)). The QML component tests call `declare_metatypes()` themselves and use substitute objects.
- **QML-owned state has no data-layer authority.** `VisualSettingsManager` and `SettingsDialog` write their `Settings` groups directly, a departure from [ADR 0010](../decisions/0010-one-data-layer-authority-for-each-body-of-persisted-state.md), which covers them. Nothing validates or publishes these values, and no C++ consumer can reach them through an interface.
- **QML-owned state has no migration path.** Renaming a property or changing the meaning of a key in `VisualSettingsManager` orphans or misreads existing values silently. Changing the identity of a series id on the C++ side has the same effect on `graphSeriesVisibility`.
- **The graph keys exist in C++ and in QML.** `VisualSettingsManager.graphVisibilityProperties` names the six graph keys by string, and nothing links it to `GraphKey` in C++. A new graph needs an entry there and its UI wiring, or it is unreachable. `GraphCatalog.js` is a second QML copy of the graph and series vocabulary; [Graph configuration][graph-configuration] owns the catalogue and its failure modes.
- **Application identity and the default `QSettings` format must be established before QML `Settings` objects are created.** `App::start()` creates those objects when it loads `Main`. Setting the identity and format first in `main()` keeps QML state in the file used by `SettingsService`. `SettingsService` passes its own format and names, so its choice of file does not depend on that order. Moving `start()` ahead of the identity and format calls, or changing the application name on only one side, would split the store.
- **A window that can veto closing must cooperate with quit.** The capture preview documents why it accepts when hidden: a refusal cancels `Qt.quit()` and leaves the process with no window. A new window with a close veto needs the same care.

## Evidence and limitations

Verified by reading: `src/main.cpp`, `src/app/app.{h,cpp}` (`initialProperties`, `start`), `src/qml_registration.h`, `src/ui/CMakeLists.txt` (module declaration, source list and include-directory block), `src/ui/qml/Main.qml`, `ScenarioWorkspace.qml`, `VisualSettingsManager.qml`, `DashboardGraphCanvas.qml`, the head of `GraphGear.qml` and `Theme.qml`, the `Settings` block and open, close and hide handlers of `SettingsDialog.qml`, the head of `CapturePreviewDialog.qml`, the `QML_ELEMENT` macros and property and invokable counts of the view-model headers, `capture_appearance.cpp`, the settings key constants in `settings_service.cpp`, and the headers of the two registration-related tests. Re-read at v1.1.0: `src/qml_registration.h`, the `QML_ELEMENT` and `QML_UNCREATABLE` macros under `src/ui`, `GraphCatalog.js` (key lists and catalogue tables), `GraphCanvasWithTooltip.qml`, `BenchmarkEditorTable.qml` and the `property var` declarations in `Main.qml`. The `file:///` versus `qrc:/` concern comes from the changelog fragment named above, not from code comments or tests, and was not reproduced; the two typed local handles (`BenchmarkEditorTable.activeEditor`, `SettingsDialog.focusedField`) were read in source. The `declare_metatypes()` call sites come from a repository grep that excluded the `.claude/worktrees` and `.worktrees` copies. This view's verification boundary is the files listed here, not the whole QML tree.

Not established:

- The remaining QML (series, benchmark, scenario list and common components) was not read; patterns in the table are drawn from the files named above and may have exceptions elsewhere.
- Whether duplicate registration (macro plus explicit call) is harmless, and what `qmltyperegistrar` emits for the include-directory rule, were not inspected; both rest on the repository's own comments.
- How QtCore `Settings` schedules writes (on change or on a timer) was not examined; `SettingsDialog` calls `sync()` explicitly when it hides.
- Whether two `QSettings` instances (one inside `SettingsService`, one per QML `Settings`) interleave safely on the same INI file was not examined.
- No test was found that asserts every `ApplicationWindow` sets `palette: Theme.palette`; that statement is from reading the four files.
This view contains no Mermaid diagram.

<!-- arch-doc:references:start -->
[benchmark-editing]: benchmark-editing.md
[composition-root]: composition-root.md
[graph-configuration]: graph-configuration.md
[graph-presentation]: graph-presentation.md
[image-capture]: image-capture.md
[run-selection-and-rendering]: run-selection-and-rendering.md
<!-- arch-doc:references:end -->
