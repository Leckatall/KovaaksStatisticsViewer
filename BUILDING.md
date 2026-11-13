# Building KovaaK's Statistics Viewer

The app is C++20 with Qt 6 / QML, built with CMake and Ninja on Windows. It builds with either MinGW (the default) or MSVC.

## Prerequisites

- [Qt 6.12](https://www.qt.io/download-qt-installer), with the `mingw_64` or `msvc2022_64` kit
- A matching compiler: MinGW (for example the one bundled with Qt or CLion) or Visual Studio 2022 or later
- CMake 3.25 or later, and Ninja
- [vcpkg](https://vcpkg.io/) with `protobuf` and `gtest` installed for your triplet:
  ```bash
  vcpkg install protobuf gtest --triplet x64-mingw-dynamic
  ```
  Use `--triplet x64-windows` for MSVC.
- [uv](https://docs.astral.sh/uv/), which runs the build script

## Configure your toolchain

The build scripts don't search for tools; they read every path from env files in `scripts/`. The tracked files contain `<...>` placeholders, and you replace them in git-ignored local files:

| File | Tracked | Holds |
|---|---|---|
| `toolchain.defaults.env` | yes | `KSV_TOOLCHAIN` (which compiler profile to use) and build-runner settings |
| `toolchain.mingw.env`, `toolchain.msvc.env` | yes | Each profile's vcpkg triplet, and placeholders for its paths |
| `toolchain.local.env` | no | Your paths that both profiles share, such as CMake and Ninja |
| `toolchain.local.mingw.env`, `toolchain.local.msvc.env` | no | Your paths for one profile |

Later files in the table override earlier ones, and environment variables override all of them. The build stops with an error naming any setting that is still a placeholder.

For a MinGW build, create `scripts/toolchain.local.env`:

```env
KSV_CMAKE_EXE=C:/path/to/cmake.exe
KSV_NINJA_EXE=C:/path/to/ninja.exe
```

and `scripts/toolchain.local.mingw.env`:

```env
KSV_C_COMPILER=C:/path/to/mingw/bin/gcc.exe
KSV_CXX_COMPILER=C:/path/to/mingw/bin/c++.exe
KSV_VCPKG_TOOLCHAIN=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
KSV_VCPKG_BIN_DIR=C:/path/to/vcpkg/installed/x64-mingw-dynamic/bin
KSV_PROTOC_EXE=C:/path/to/vcpkg/installed/x64-mingw-dynamic/tools/protobuf/protoc.exe
KSV_QT_BIN_DIR=C:/Qt/6.12.0/mingw_64/bin
KSV_QT6_DIR=C:/Qt/6.12.0/mingw_64/lib/cmake/Qt6
```

For MSVC, fill in `scripts/toolchain.local.msvc.env` with the same keys, pointing the compilers at `cl.exe`, the vcpkg paths at the `x64-windows` triplet and Qt at `msvc2022_64`, and add:

```env
KSV_VCVARS_BAT=C:/Program Files/Microsoft Visual Studio/<version>/<edition>/VC/Auxiliary/Build/vcvars64.bat
```

The build scripts load the MSVC environment from that file themselves, so you don't need a Developer shell. To make MSVC your default, put `KSV_TOOLCHAIN=msvc` in `toolchain.local.env`. To use it for a single run, set the `KSV_TOOLCHAIN` environment variable instead.

If you use git worktrees, a worktree without its own local files uses the main checkout's.

## Build and test

```bash
uv run python scripts/build_and_test.py
```

This configures a Debug build in `build-agent/`, builds every target and runs the full test suite. A successful run prints a single line, and a failure prints every failing test and compiler diagnostic. Full logs are kept in `.temp/build-and-test/`.

Options:

- `--build-only` builds without running the tests.
- `--scope <scope>` runs one part of the suite: `domain`, `data`, `qt-data`, `app`, `ui`, `ui-cpp`, `qml` or `integration`.
- `--match '<wildcard>'` narrows a scope to matching test names, and `--list` shows the names without running anything.
- `--scope qmllint` runs qmllint over the QML.

The app is built as `build-agent/ksv.exe`. It's renamed to `KovaaksStatsViewer.exe` only when packaged.

## Package a release

```powershell
./scripts/package-release.ps1
```

This builds a Release from scratch, bundles Qt, vcpkg and compiler runtime DLLs next to the executable, smoke-tests it with your toolchain removed from `PATH`, and zips it as `dist/KovaaksStatsViewer-<version>-win64.zip`. Pass `-Toolchain msvc` to package with MSVC, or `-ValidateOnly` to check the configuration without building.

## Developer tools

`tools/gallery` builds `ksv_gallery`, which runs the real app against a throwaway KovaaK's folder and hot-reloads edits to `src/ui/qml/`. It's on by default in Debug builds (`KSV_BUILD_GALLERY`) and never shipped.

## Project structure

```
src/
  domain/    Core model (runs, scenarios, benchmarks). No Qt, no I/O.
  data/      Reading .perf and stats files, the profile store
  qt_data/   Qt-backed services: settings, files, playlists, images
  app/       Use cases, and App, which wires everything together
  ui/        View models and the QML interface
tests/       Tests for each layer, plus integration tests against real .perf files
tools/       Developer tools, e.g. a component gallery with QML hot reload
docs/        Architecture description, decision records and feature designs
```

Each layer only depends on the layers below it, through interfaces. See [docs/architecture/](docs/architecture/README.md) for the full description and the decisions behind it.
