---
type: added
area: Architecture
---
`src/logging_setup.h` — header-only `ksv::install_debug_message_pattern()`, calls
`qSetMessagePattern` with `%{file}:%{line} %{function} - %{message}`.
Called first in `main()` in both `src/main.cpp` and `tools/gallery/gallery_main.cpp`,
so every existing `qDebug()`/`qWarning()`/`qCritical()` call site carries its call-site
context automatically with no per-call-site changes.
