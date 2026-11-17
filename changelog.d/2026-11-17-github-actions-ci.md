---
type: added
area: Build & packaging
---
`.github/workflows/ci.yml` — GitHub Actions CI on push to `main`, pull requests and manual dispatch, on `windows-2025`.
Build job: `jurplel/install-qt-action` installs Qt 6.12.0 `win64_mingw` plus Qt's own `tools_mingw1510` (GCC 15.1, the compiler the `mingw_64` kit is built with), CMake and Ninja; that MinGW is prepended to PATH so neither vcpkg nor the build picks up the image's other GCCs.
vcpkg runs in classic mode at a pinned commit (`VCPKG_COMMIT`, matching the local install: protobuf 6.33.4, abseil 20260107.1, gtest 1.17.0) with `--host-triplet x64-mingw-dynamic`, and its built packages are kept in a `files` binary cache stored with `actions/cache`.
The `KSV_*` toolchain keys are set as environment variables, which override the tracked env files, and the job runs `scripts/build_and_test.py` then `--scope qmllint`; `.temp/build-and-test/` is uploaded on failure.
Separate `script-tests` job runs the `scripts/tests` pytest suite.
