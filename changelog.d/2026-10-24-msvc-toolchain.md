---
type: changed
area: Build & packaging
---
Toolchain moved from MinGW to MSVC (VS 2026, toolset 14.51) with Qt 6.12.0 `msvc2022_64` (binary-compatible with the newer toolset) and the `x64-windows` vcpkg triplet; MinGW is no longer supported.
New toolchain key `KSV_VCVARS_BAT`: `build_runner.config.developer_environment` runs it through `cmd /u` and overlays the captured environment (keys upper-cased, since cmd prints `Path`) on the runner's process environment, and `Import-KsvVcVars` in `toolchain-config.ps1` does the same for `package-release.ps1`, so neither needs a Developer shell.
The toolchain fingerprint (schema 2) now hashes the vcvars script and every DLL in the vcpkg `bin/` instead of hard-coded MinGW and `lib*`-prefixed vcpkg DLL names.
Removed `ksv_copy_mingw_runtime_dlls` and its calls on every executable: the MinGW runtime-DLL collisions it guarded against do not exist with the MSVC runtime.
`src/data/CMakeLists.txt` no longer stages a build-local protoc runtime; `protobuf_generate` uses `protobuf::protoc` directly, because the `x64-windows` port ships `abseil_dll.dll`/`libprotobuf.dll`/`libprotoc.dll` in `tools/protobuf` beside it.
Packaging copies the CRT DLLs app-locally from whichever `Microsoft.VC14*.CRT` folder `VCToolsRedistDir` holds and runs windeployqt with `--no-compiler-runtime`; the smoke test requires `vcruntime140.dll`/`msvcp140.dll` to load from `dist/`, since on a developer machine System32 would otherwise satisfy them and hide a missing copy. `-MingwBinDir` is replaced by `-VcVarsBat`.
