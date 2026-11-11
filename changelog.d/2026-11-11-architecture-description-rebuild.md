---
type: internal
area: Architecture
---
The Architecture Description is rebuilt from the code at v1.0.0, replacing the out-of-date root and its two views. The root now carries `Documented version: v1.0.0`, and every view sits flat in `docs/architecture/views/`.
`profile-lifecycle` and `run-selection-and-rendering` are rewritten and move out of `views/runtime/` with their IDs unchanged (`profile-lifecycle` now declares only the `runtime` viewpoint). Eleven views are new: `composition-root`, `run-history-data`, `graph-presentation`, `graph-configuration`, `qml-boundary`, `image-capture`, `build-and-deployment`, and the `benchmarks` parent view with its children `benchmark-definitions`, `benchmark-tracking` and `benchmark-editing`.
Diagrams follow the current diagram conventions: titles set inside the Mermaid block, scopes that state their selection rule, and notation-only keys.
