---
type: removed
area: Benchmarks
user: Editing and saving existing benchmarks no longer interrupts you with history recalculation confirmations.
---
Removed BenchmarkManagerDialog's retrospective confirmation gate, which separately intercepted scenario mapping or identity edits and saving library benchmarks. These actions now proceed directly while preserving active-editor commits, save error handling, pending transitions, and the unsaved-changes guard. Invalid threshold consequences remain visible in the editor's nonmodal warning.
