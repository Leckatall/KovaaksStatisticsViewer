---
type: fixed
area: Build & packaging
---
CMakeLists.txt is now the single source of the version: `KSV_VERSION_SUFFIX` carries the pre-release
suffix `project(VERSION)` cannot hold, and the new `scripts/version.ps1` is the only thing that parses
it, so packaging and future release automation share one reader instead of growing their own copies.

`package-release.ps1` fixes, in order of consequence:

- `$RepoRoot` was a param default of `Resolve-Path "$PSScriptRoot/.."`, and `powershell -File`
  evaluates param defaults *before* `$PSScriptRoot` is populated - so under `-File` it resolved to the
  root of the current drive and the script built into `C:\build-release` and packaged into `C:\dist`.
  It only ever worked when invoked as `& ./scripts/package-release.ps1`. `$RepoRoot` and `$BuildDir`
  are now resolved in the body.
- The build directory is deleted before configuring unless `-Incremental` is passed. A reused
  `build-release/` can silently package a stale app: CMake honours `CMAKE_TOOLCHAIN_FILE`/compiler/
  generator only on first configure, and stale AUTOMOC/AUTORCC/qmlcachegen output can leave the
  previous QML baked into the binary. Both report success. A post-configure assertion reads
  `CMAKE_BUILD_TYPE`, `VCPKG_TARGET_TRIPLET` and `CMAKE_CXX_COMPILER` back out of `CMakeCache.txt`,
  which is what catches the first case when `-Incremental` is used.
- The zip archived the dist folder's *contents*, so extracting scattered ~1500 loose files into the
  user's chosen directory. It now archives the folder. Entries are also written one at a time with
  their names normalised to `/`: on .NET Framework, which is what Windows PowerShell 5.1 runs on, both
  `Compress-Archive` and `ZipFile.CreateFromDirectory` write backslashes into entry names, which the
  ZIP spec forbids and which makes `unzip` on Linux/macOS extract the tree as flat files.
- A missing MinGW runtime DLL was a `Write-Warning`; the comment above it documents that exact
  condition as what breaks startup on a clean PC, so it now throws.
- New smoke test (on by default, `-SkipSmokeTest` opts out): launches the packaged exe from `dist/`
  with the toolchain directories stripped from `PATH`. **The pass condition is the module list, not
  survival** - when a DLL is missing the loader raises a hard error and Windows keeps the doomed
  process alive behind a message box, so a process that never loaded Qt looks identical to a healthy
  one if you only ask whether it is still running (verified: ~34 modules vs ~90). It requires the Qt
  stack, `libprotobuf`/`libabseil_dll` and the MinGW runtime to be loaded. `windeployqt` ships only
  `qwindows.dll`, so `qoffscreen.dll` is borrowed from the Qt install for the test and removed before
  zipping.
- `-ValidateOnly` runs just the tool-path assertions, so automation can check the environment before
  committing and tagging. A top-level `trap` makes the exit code explicit rather than inherited.
- Dropped `--config Release`; Ninja is single-config and takes the build type from the cache.

`.ps1` files under `scripts/` must stay ASCII: Windows PowerShell 5.1 reads a BOM-less script as ANSI,
so a UTF-8 em dash decodes to a curly quote, which 5.1 accepts as a string delimiter - the literal ends
early and the parse errors surface far from the offending character.
