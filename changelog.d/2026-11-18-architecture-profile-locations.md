---
type: internal
area: Architecture
---
The Architecture Description reflects `ProfileLocationsUseCase`: the root's building-block diagram drops the presentation-to-data bypass, `composition-root` adds the use case to the construction order and subscriber lists, and `build-and-deployment` drops the `ksv_ui → ksv_data` header dependency.
