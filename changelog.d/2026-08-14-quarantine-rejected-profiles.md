---
type: fixed
area: Data & Profile
user: A profile file that cannot be read is now kept aside instead of being overwritten.
---
`ProfileSerializer` quarantines unparseable and version-mismatched files with timestamped,
content-derived names before profile rebuilding begins.
