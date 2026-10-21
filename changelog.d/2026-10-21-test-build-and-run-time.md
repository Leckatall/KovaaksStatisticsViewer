---
type: internal
area: Build & packaging
---
Cut test build and run time. Measured on a quiet machine; numbers are before → after each change.

- `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)`: test executables no longer run at build time; about 0.2 s off each test relink, nothing else measurable. Superseded by the next change.
- One CTest test per GoogleTest executable (`ksv_add_gtest_test` in `tests/CMakeLists.txt`) writing `build-agent/tests/gtest-reports/<target>.xml`, instead of one CTest test per case. The build runner expands a failing executable into its failing cases from that report, falls back to captured output for a crash, and still reports the case count (1506). `--scope all` test phase 137 s → 75 s.
- `ctest -j4` in the runner's `--scope all` run: test phase 66 s → 23 s (`-j8` measured 24 s; `integration_tests` at about 22 s is the critical path).
- Precompiled headers on `integration_tests`, `ui_tests`, `app_tests` and `qt_data_tests` (gtest plus each target's common Qt headers, and `app/app.h` for integration): clean build span 251.5 s → 225.5 s, test-TU compile CPU 907 s → 643 s, touching one integration test file 21.9 s → 16.7 s. Touching a data test file is unchanged.

Tried and reverted: merging the ten `tests/data` executables into one `data_tests`. No measurable gain in clean build or test time, and touching one data test file got about 1 s slower (relink 0.6 s → 1.4 s).

Overall, clean build plus full `--scope all`: 434 s → 273 s; full `--scope all` after a no-op build: about 28 s.
