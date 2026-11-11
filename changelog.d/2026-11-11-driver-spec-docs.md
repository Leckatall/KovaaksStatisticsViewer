---
type: internal
area: Build & packaging
---
`tools/driver/README.md` - documents modelled scenario entries (references, `idealScore`, `skill`/`progression`, `handRuns`, shape-then-scale generation, seeding), the scenario-spec and performance-spec formats, and the Viscose fixture row.
`tools/driver/specs/progression-example.json` - worked example: VT Controlsphere (constant fire) with a late-fade shape, day progression with day and run fluctuation and a hand-specified choke run, plus Pasu Angelic borrowing the 1w3ts Reload model through `idealScore`; it copies the Viscose benchmark so both rows rank on the real ladder.
`specs/scenarios/` (VT Controlsphere, 1w3ts Reload) and `specs/performances/` (late-fade, choke) hold the referenced specs; `DriverSpecTest.CheckedInSpecsBuild` builds or parses every checked-in spec.
