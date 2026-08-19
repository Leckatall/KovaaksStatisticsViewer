---
type: internal
area: Settings
---
Removed the dead `PersistenceWriteFailed` branch from `SeriesConfigStore::commitLocked` — `ISettingsService::setSeriesConfigDocument` returns `void`, so `writeLocked` could never report failure; `writeLocked` now returns `void`.
Added `target_compile_definitions(ksv PRIVATE $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>)` to the top-level `CMakeLists.txt`, gating `qDebug()` calls (series config store seed/quarantine logging, `file_service.cpp`) out of Release builds.
