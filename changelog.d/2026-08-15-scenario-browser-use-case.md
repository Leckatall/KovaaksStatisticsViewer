---
type: internal
area: Architecture
---
`ScenarioBrowserUseCase` now owns scenario-browser aggregation, selected-run access, and refresh notifications; `ScenarioBrowserViewModel` depends only on its narrow contract instead of `ISessionController`.
