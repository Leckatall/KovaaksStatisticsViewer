---
type: internal
area: Architecture
---
`src/ui/presentation/` split into `graphs/` (with `common/axes/` and `common/series/`), `expression/`,
`benchmarks/` (`manager/`, `tracking/`), `scenarios/` and `app/`; `tests/ui/` mirrors it. Includes are now
`presentation/<dir>/…`, and the bare `presentation/` include directory is gone.
