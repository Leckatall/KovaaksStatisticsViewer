---
type: added
area: Build & packaging
---
`KSV_VERSION` (already computed from `project()` + `KSV_VERSION_SUFFIX` in [CMakeLists.txt](../CMakeLists.txt))
is now passed to the `ksv` target as a compile definition and fed into
`QCoreApplication::setApplicationVersion()`/`QGuiApplication::setApplicationVersion()` in
[main.cpp](../src/main.cpp), so it's readable at runtime as `Qt.application.version` from QML. No UI
consumes it yet.
