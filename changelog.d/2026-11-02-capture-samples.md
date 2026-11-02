---
type: internal
area: Benchmarks
---
`capture_samples_test.cpp` writes the eight review PNGs when `KSV_CAPTURE_SAMPLES_DIR` is set on the real Windows platform, and skips otherwise. Table header, bar and summary heights now follow the capture font, so a large font stays inside the layout.
