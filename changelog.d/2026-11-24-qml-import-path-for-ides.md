---
type: fixed
area: Build & packaging
---
IDE QML code models flagged every project component and `QML_ELEMENT` type as unknown (Qt Creator: `Unknown component (M300)`). Every QML file reaches the project's types through `import KovaaksStatsViewer`, and a code model resolves that by finding `KovaaksStatsViewer/qmldir` under one of its import paths: Qt's `qml` directory, the build root, and the `QML_IMPORT_PATH` cache entry. `qt_add_qml_module(ksv_ui ... OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer)` puts the module under `<build>/src/ui/`, which is none of those, and the commented-out `set(QML_IMPORT_PATH ...)` in `src/ui/CMakeLists.txt` named the module directory itself rather than its parent and never reached the cache.
`src/ui/CMakeLists.txt` now caches `QML_IMPORT_PATH` as `${CMAKE_CURRENT_BINARY_DIR}`, the module's parent, without FORCE so a value passed at configure time still wins. Runtime loading is unaffected: the module is compiled in and loaded from resources.
