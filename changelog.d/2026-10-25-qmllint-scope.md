---
type: added
area: Build & packaging
---
`scripts/build_and_test.py --scope qmllint` — builds Qt's `all_qmllint_json` target and reads the `<build dir>/*_qmllint.json` reports it writes, rather than scraping qmllint's console output.
New `scripts/build_runner/qmllint.py` parses the reports and renders them keyed: `F<n>` per file, `R<n>` per distinct (type, category, message), one line per occurrence carrying qmllint's suggested edit.
qmllint emits a finding's supporting "Note:" entries as separate warnings after it; the parser folds each into the finding it follows, so they are neither counted nor listed as findings of their own.
qmllint exits zero on warnings, so the scope decides failure itself: anything above `info`/`debug` fails. A failed build with no failing finding is reported as a build failure (a dependency such as qmltyperegistrar did not compile, or qmllint crashed).
Stale reports are deleted before the build so a qmllint that writes nothing is never read as passing from the last run.
Opt-in only: the no-scope regression run does not lint, since the QML currently has 453 findings.
