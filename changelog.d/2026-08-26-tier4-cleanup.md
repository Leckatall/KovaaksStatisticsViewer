---
type: changed
area: Architecture
---
`IScenarioBrowserUseCase::onChanged` drops its `QObject*` context parameter, matching
`ISeriesManagementUseCase`'s callback-only signature; `i_scenario_browser_use_case.h` is now
Qt-free and covered by the `contracts_qt_free` header-set guard. Plus small CMake/QML-registration
consistency fixes.
