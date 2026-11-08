---
type: changed
area: Build & packaging
---
MinGW is supported again and is the default toolchain; MSVC stays available. An MSVC Debug build links Qt's debug DLLs, the debug CRT and checked iterators, while Qt's `mingw_64` package is release-only, so MinGW Debug builds, tests and the app run much faster.
New toolchain key `KSV_TOOLCHAIN` (`mingw` or `msvc`, tracked default `mingw`) picks a tracked compiler profile, `scripts/toolchain.mingw.env` or `scripts/toolchain.msvc.env`, layered between `toolchain.defaults.env` and `toolchain.local.env`. `KSV_VCVARS_BAT` is required only for `msvc`; a MinGW build never runs vcvars. `build_and_test.py`, the UI driver and `package-release.ps1` (new `-Toolchain`, restored `-MingwBinDir`) all resolve it the same way.
Toolchain fingerprint schema 3 records the flavour and, for MinGW, hashes the compiler's runtime DLLs again, so switching flavour rebuilds `build-agent/` from cold. MinGW builds put the compiler directory first on the runner's PATH again.
`ksv_copy_mingw_runtime_dlls` is back on every executable but is a no-op outside MinGW, and the build-local protoc runtime in `src/data/CMakeLists.txt` is staged only under MinGW. The `Qt6_DIR` fallback follows the compiler.
Packaging under MinGW runs windeployqt with `--compiler-runtime`, force-copies the compiler-matched MinGW runtime DLLs last and requires them, plus `libabseil_dll.dll`, in the smoke test. MSVC packaging is unchanged.
