---
type: internal
area: Build & packaging
---
`ksv_driver` and `ksv_gallery` now call `QCoreApplication::setApplicationVersion(KSV_VERSION)` like `src/main.cpp`, so the About dialog in driver screenshots reads "Version 0.6.0-beta" instead of a bare "Version".
Both targets get `KSV_VERSION="${KSV_VERSION}"` as a private compile definition from the variable the root `CMakeLists.txt` sets before `add_subdirectory(tools/...)`, so the root file stays the single source of the version.
