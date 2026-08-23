---
type: internal
area: Settings
---
`SeriesExpressionEditorModel` now owns a typed `EditableExpressionNode` tree and exposes structural pointer-based editing with targeted root and selection notifications. Serialization now walks typed nodes; model tests cover ownership-preserving wrapping, persistence round-trips, and notification timing.
