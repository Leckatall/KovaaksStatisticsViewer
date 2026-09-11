---
type: internal
area: Build & packaging
---
`scripts/build_runner` now reports failures from the test frameworks' own machine-readable output instead of scraping console text, and renders one compressed report so no log needs opening.
New `parsers.py` reads GoogleTest XML (already written by `--gtest_output=xml:` but previously ignored), the Qt Test plain-text report, and `ctest --output-junit`, which is newly passed on the `--scope all` invocation.
Qt's `junitxml` logger is deliberately not used: it carries no failure location and splits a multi-line message between the `message` attribute and the CDATA body, so `parse_qt_txt` parses the text report instead — an incident there is unambiguously terminated by its `... : failure location` line.
New `failures.py` holds the report model (`Failure`, `Diagnostic`) and renderer. Compression is by keying, never by capping: `R{n}` keys a failure *reason*, collapsing tests that share a location and message into one block that still names every test; `T{n}` keys a build target; the repository root is stated once in the header; test names factor their shared suite prefix and their data-driven tags (`test_f(empty|full)`).
`diagnostics.py` keeps the text reducers as the fallback for a runner that died before writing a report, and for configure/build output, which has none. `reduce_build_failure` now returns deduplicated `Diagnostic`s carrying every target that reproduced them, so a broken header is reported once rather than once per object; within a target, only the first diagnostic keeps its source excerpt, because the compiler's later errors in that translation unit are consequences of the first.
`_fallback`'s unbounded 160-line raw dump is gone; unrecognized output keeps a bounded tail explicitly labelled as unparsed.
Per-phase `Full log:` lines are replaced by one trailing pointer worded as a last resort.
Measured on captured runs: a 17-test QML failure with one root cause went from 6799 to 1054 characters, and a header syntax error cascading to 120 diagnostics from 63888 to ~18000, with every test name, diagnostic and location retained.
