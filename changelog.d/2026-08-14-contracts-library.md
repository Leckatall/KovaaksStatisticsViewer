---
type: internal
area: Architecture
---
`ksv_contracts` makes the app-to-presentation contract explicit and Qt-free. Its CMake-generated header verifier is `EXCLUDE_FROM_ALL`, so CTest builds it to preserve the boundary.
