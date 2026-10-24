---
type: fixed
area: Settings
user: The expression editor opens again instead of failing with an "Unknown method return type" error.
---
`SettingsViewModel::beginExpressionEdit` returned a type moc only saw forward-declared, so QML resolved it by name at call time and broke once f05cb36 dropped the incidental `SeriesExpressionEditorModel` registration. `settings_vm.h` now has `Q_MOC_INCLUDE` for it. The new `ui_metatype_tests` executable checks that every property and method type on the QML-facing classes resolves without `declare_metatypes()`.
