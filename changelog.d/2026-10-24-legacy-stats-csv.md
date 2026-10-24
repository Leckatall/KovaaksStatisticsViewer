---
type: fixed
area: Data & Profile
user: Stats files from KovaaK's versions before 3.1 (mid-2022 and earlier) now import instead of failing.
---
`StatsCsvParser` rejected every CSV written before 3.1.0 because it required `Hit Count`/`Miss Count` (added in 3.1.0, 2022-08-04) and, before 2.0.3.0 (2020-12-10), `Challenge Start`. The parser now reads `Game Version` (forms seen: `2.0.1.2`, `v2.8.8`, `3.0.2`, `3.9.10.<build-stamp>`) and gates fallbacks on it rather than on key absence:

- Below 3.1.0, hits and misses come from the weapon summary (summed over every weapon row). The per-kill rows are not used, because they omit shots fired after the last kill.
- Below 2.0.3.0, and for 3.0.x (which writes the placeholder `H:M:S.s`), the filename's end time stands in for the start. Those runs therefore get a zero scenario length and add no playtime. Their run keys are late by the run's own length, so the keys stay unique.
- Below 3.1.0, `Pause Count`/`Pause Duration` are ignored. Their unit isn't seconds (the duration often exceeds the whole run), and 3.0.x writes `INT_MIN`. Either value would have corrupted the derived run length.

New `endTimeOfDay()` in `run_filename.h`. Fixtures: a 2.0.1.0 CSV (2020) and a 2.0.5.3 CSV (2021) in `tests/examples/`. Against a real stats folder of 14,462 CSVs, the only one still rejected is an all-whitespace corrupt file.
