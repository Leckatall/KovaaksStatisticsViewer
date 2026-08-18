---
type: changed
area: Graphing
user: Main graph series are now resolved through the persisted series configuration and presented with stable identities.
---
`GraphUseCase` now owns series resolution and mutations, while `GraphViewModel` adapts resolved results and transitional QML controls to Qt types. Application, gallery, and integration composition roots use the real series store and average evaluator.
