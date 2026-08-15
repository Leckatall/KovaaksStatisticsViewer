---
type: changed
area: Data & Profile
---
`IProfileSerializer::load` now returns `ProfileLoadResult`, preserving a loaded profile or identifying an absent, unparseable, or schema-version-mismatched store.
`ProfileSerializer` exposes the same rejection reason it already uses when quarantining invalid store files;
`ProfileService` retains its existing fallback behavior for every rejection.
