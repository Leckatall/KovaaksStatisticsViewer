---
type: internal
area: Documentation
---
Root style (`docs/conventions/architecture-description/style/root.md`) now requires a `Documented version: v<version>` line near the top of the architecture root, recording the release it was last completely reconciled against.
The changes since that version are the starting point for the next full reconciliation.
`scripts/arch_doc.py validate document` reports a missing, malformed or repeated line as a structural failure (lines inside code fences do not count), and `structure.md` lists it among the structural failures.
The current root has no such line yet, so validation fails until the next full reconciliation sets it.
