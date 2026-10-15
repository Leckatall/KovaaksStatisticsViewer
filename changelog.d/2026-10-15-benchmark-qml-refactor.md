---
type: internal
area: User Interface
---
Phase 0: `src/ui/qml/` regrouped by feature with `git mv` — `shell/`, `scenario/`, `graph/`, `series/`, `benchmark/`, `common/`; `Main.qml` stays at the root because the integration tests read it by path.
`QML_FILES` in `src/ui/CMakeLists.txt` updated, icon paths in moved files now `../icons/…`.
Every QML file in the module gains `import KovaaksStatsViewer`: the compiled module already sees cross-folder types through the per-directory qmldirs (`QTP0004`), but `ksv_gallery`, `ksv_driver` and the QML test suites load files from disk, where a file implicitly imports only its own directory.
The QML test suites' directory imports (`import "../../../src/ui/qml"`) now name the subfolder holding the types each test uses.
A1: group-structure rules the QML derived from raw `groups` arrays are computed in C++. `BenchmarkManagerViewModel::groups` entries carry `acceptsScenarios` (and, on categories, `subcategoryNeedsRelocation`), a new `assignTargets` property lists Uncategorized then every group that accepts scenarios, `BenchmarkTableModel::rowInfo()` adds `positionInContainer` / `containerSize`, and `BenchmarkProgressGridModel::groups` entries carry `spansSubcategory`.
`canDropOn()`, `assignTargets()`, `currentRowPlacement()`, `requestSubcategory()` and the grid's `spansSubcategory()` now read those fields; GoogleTests cover each, and the plain-JS test doubles supply them.
A2: `BenchmarkEditorTable`'s `addCategoryRequested` / `addSubcategoryRequested` / `addRankRequested` signals and the `creationAccepted` flag the dialog wrote back are replaced by one `creationHandler` property, a synchronous `(request) => bool` taking `{kind: "category" | "subcategory" | "rank", …}`; with no handler set, creation is rejected. The Manager dialog dispatches it to its existing `addCategory` / `requestSubcategory()` / `addTier` logic.
