---
type: fixed
area: Build & packaging
---
Compiling `resources/app-icon/ksv.rc` failed with `'C:\Program' is not recognized` whenever the windres command line contained a quoted argument. windres runs its preprocessor through `cmd.exe`, and cmd's quote handling then splits the unquoted `C:\Program Files\JetBrains\...` toolchain path. CMake produced such quotes from the `KSV_VERSION="..."` define and from any include directory containing a space (the `vcpkg (1)` include directory), both of which CMake passes to the resource compiler too.
The root `CMakeLists.txt` now overrides `CMAKE_RC_COMPILE_OBJECT` for windres to `-O coff <FLAGS> <SOURCE> <OBJECT>`, dropping `<DEFINES>` and `<INCLUDES>`; `ksv.rc` only embeds an icon and needs neither.