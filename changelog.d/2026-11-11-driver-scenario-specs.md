---
type: internal
area: Build & packaging
---
`gallery::makeModelledRun` (`tools/gallery/modelled_run.{h,cpp}`) - a second run generator beside `gallery::makeRun`, which is unchanged. It builds the shaped, noisy ideal run first, then bisects one level that scales shot rate and accuracy (split by the scenario's skill weights in log space) until the run scores `skill x idealScore`, then quantises with error diffusion so hits never exceed shots. No accuracy clamp beyond that; partial first and last seconds are kept.
`SpecRandom` - splitmix64 plus a hand-written Box-Muller, because standard library distributions differ between MinGW and MSVC and a spec must always write the same bytes.
`driver::parseScenarioModel` / `driver::parsePerformanceShape` - strict field-path parsing of scenario-spec and performance-spec JSON (scoring per hit, kill, miss and damage, hits per kill, ideal shot rate and accuracy, skill weights and a shot-rate cap; windows and per-second noise).
