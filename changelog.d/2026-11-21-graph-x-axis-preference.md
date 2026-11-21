---
type: internal
area: Graphing
---
New `AxisOrientation { Horizontal, Vertical }`; `GraphAxisDescriptor` is now `{ key, orientation, std::optional<AxisPolicy> defaultPolicy }`, the policy set only for an axis whose range the user tunes. Every existing axis is `Vertical` with its policy; the two rank graphs gain two `Horizontal` axes without a policy, `kRunXAxis` ("runs") and `kCalendarXAxis` ("calendar"). Code that handles fit policies (`defaultGraphPreferences`, policy validation, `axisPolicyFor`, the Settings draft's `axes`, `setAxisMode`) only looks at axes that have one, and a policy stored for any other key is `UnknownAxis`.
`GraphDescriptor::defaultXAxis` ("runs" for both rank graphs, empty elsewhere) and `GraphPreferences::xAxis`, set exactly when the graph offers horizontal axes. Validation reports `NotApplicable`, `MissingValue` or `UnknownAxis` in group "axes", field "xAxis".
`GraphPresentation::xAxis` carries the shown axis; `diffGraphConfigurations` reports an `xAxis` change under `style`, since it moves no measurement, range or window.
Graph configuration schema 7: the rank sections carry a required `"xAxis"` string. Documents of versions 1-6 load with each graph's default axis and are not quarantined; an unknown or missing value in a version-7 document rejects that graph's section like any other malformed section. An older build quarantines a whole version-7 document.
`SettingsViewModel::setShownXAxis(graph, axis)` stages one of a graph's horizontal axes, and each draft gains `xAxes: [{key, shown}]`; Reset stages `"runs"` through `defaultGraphPreferences`.
`AxesPanel.qml`'s inline `YAxisButton` is extracted into `graph/ShowOnAxisButton.qml`, which takes the axis letter, keeps the `showOn<letter>Axis_<key>` object names and emits `chosen(key)`; `GraphSettingsEditor.qml` shows the rank graphs' Axes section with a "Horizontal axis" block of exclusive X-axis rows, named and described from `GraphCatalog.js`.
