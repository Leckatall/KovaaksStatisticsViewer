---
type: internal
area: User Interface
---
Phase 0: `src/ui/qml/` regrouped by feature with `git mv` — `shell/`, `scenario/`, `graph/`, `series/`, `benchmark/`, `common/`; `Main.qml` stays at the root because the integration tests read it by path.
`QML_FILES` in `src/ui/CMakeLists.txt` updated, icon paths in moved files now `../icons/…`.
Every QML file in the module gains `import KovaaksStatsViewer`: the compiled module already sees cross-folder types through the per-directory qmldirs (`QTP0004`), but `ksv_gallery`, `ksv_driver` and the QML test suites load files from disk, where a file implicitly imports only its own directory.
The QML test suites' directory imports (`import "../../../src/ui/qml"`) now name the subfolder holding the types each test uses.
