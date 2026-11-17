---
type: added
area: Build & packaging
---
`.github/workflows/ci.yml` — GitHub Actions CI on push to `main`, pull requests and manual dispatch, on `windows-2025`.
Build job: `jurplel/install-qt-action` installs Qt 6.12.0 `win64_mingw`, CMake and Ninja from Qt's repository, with aqtinstall taken from a pinned `master` commit because 3.3.0 cannot resolve Qt 6.11+'s per-arch Windows repository folders.
The compiler is a pinned, SHA-256-checked winlibs GCC 15.2.0 (mingw-w64 v14, msvcrt, posix threads), cached with `actions/cache` and prepended to PATH so neither vcpkg nor the build picks up the image's other GCCs.
Qt's own `tools_mingw1510` was tried first and rejected: its mingw-w64 v12 `libmingw32.a` contains `ucrt_exit_wrappers.o`, which defines `_Exit`/`quick_exit` in terms of `__imp__Exit`/`__imp_quick_exit`, and `libmsvcrt.a` exports neither, so any msvcrt DLL calling `std::_Exit` (gtest does) fails to link.
vcpkg runs in classic mode at a pinned commit (`VCPKG_COMMIT`, matching the local install: protobuf 6.33.4, abseil 20260107.1, gtest 1.17.0) with `--host-triplet x64-mingw-dynamic`, and its built packages are kept in a `files` binary cache stored with `actions/cache`.
The `KSV_*` toolchain keys are set as environment variables, which override the tracked env files, and the job runs `scripts/build_and_test.py` then `--scope qmllint`; `.temp/build-and-test/` is uploaded on failure.
Separate `script-tests` job runs the `scripts/tests` pytest suite.
