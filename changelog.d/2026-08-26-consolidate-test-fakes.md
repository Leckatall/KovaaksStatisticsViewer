---
type: changed
area: Architecture
---
Duplicated hand-written test fakes (`FakeSettingsService`, `FakeProfileService`, `FakeFileService`,
`FakeSessionController`, `FakeSeriesConfigStore` — each previously redefined in 2–5 test files)
consolidated into a new header-only `tests/support/` target the per-layer test executables link
against.
