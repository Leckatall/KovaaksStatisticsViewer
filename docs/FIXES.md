# Outstanding Issues

Static audit findings retained from the 2026-08-13 review. Each checkbox reports an identified issue and includes a source locator for follow-up. Runtime-only claims and items that require build/test evidence are intentionally excluded.

## Domain

- [ ] `toString()` ignores `localtime_s` / `localtime_r` failure before formatting the result. (`src/domain/scenario_perf.h:82`, `ScenarioRunId::toString`)
- [x] A timestamp can drive an unbounded rolling-series vector allocation. `rollingTimeAverageFor` now walks day-by-day only while the trailing window-sum is non-zero, jumping straight to the next recorded play day once it hits zero, bounding output to `daily_totals.size() * window_days` instead of the full calendar span; covered by `GetRollingTimeAverageStaysSparseAcrossAWideTimestampGap`. (`src/domain/user_profile.cpp:160`, `UserProfile::rollingTimeAverageFor`)
- [ ] UTC day bucketing disagrees with local-time run labels at day boundaries. (`src/domain/scenario_perf.h:74`, `ScenarioRunId::startDay`; `src/domain/scenario_perf.h:82`, `ScenarioRunId::toString`)
- [ ] A scenario display name is fixed by its first insertion for a given hash. (`src/domain/user_profile.cpp:27`, `UserProfile::addScenarioPerf`; `src/domain/scenario_perf.h:53`, `ScenarioId::operator==`)
- [ ] `ScenarioPerf::scenario_length` is uninitialised by default before contributing to aggregates. (`src/domain/scenario_perf.h:96`, `ScenarioPerf`; `src/domain/user_profile.cpp:37`, `UserProfile::addScenarioPerf`)
- [x] Invalid `add_data` input can create a phantom zero data point before throwing. Type validation now runs before `get_data_point()` is called, so a rejected call leaves `data` untouched; covered by `AddDataDoesNotLeaveAPhantomPointWhenTypeValidationFails`. (`src/domain/scenario_perf.cpp:11`, `ScenarioPerf::add_data`; `src/domain/scenario_perf.cpp:43`, `ScenarioPerf::get_data_point`)
- [ ] `getAllRunRecords()` exposes a vector reference that later insertions can invalidate. (`src/domain/user_profile.cpp:188`, `UserProfile::getAllRunRecords`; `src/domain/user_profile.cpp:24`, `UserProfile::addScenarioPerf`)
- [ ] `getAverageScore()` uses a float accumulator with avoidable precision drift. (`src/domain/user_profile.cpp:83`, `UserProfile::getAverageScore`)
- [ ] The rolling-window residual can become slightly negative through floating-point arithmetic. (`src/domain/user_profile.cpp:177`, `UserProfile::rollingTimeAverageFor`)
- [ ] `ScenarioRunId` uses a weak XOR hash combine. (`src/domain/scenario_perf.h:140`, `std::hash<ScenarioRunId>::operator()`)
- [ ] Linear data-point lookup makes decoding a run quadratic in distinct point count. (`src/domain/scenario_perf.cpp:43`, `ScenarioPerf::get_data_point`; `src/data/formats/protobuf/proto_decoder.cpp:40`, `ProtoDecoder::decode`)
- [ ] The local-time formatting test duplicates the implementation it tests. (`tests/domain/domain_test.cpp:174`, `ToStringFormatsNameDateAndTimeInLocalTime`)

## Data and profile store

- [ ] A file observed mid-write can be cached as a bad run and permanently marked known. (`src/qt_data/file_service.cpp:37`, `FileService::handleDirectoryChanged`; `src/data/formats/protobuf/proto_decoder.cpp:27`, `ProtoDecoder::decode_file`)
- [ ] `decode_file()` does not check whether its input stream opened and treats parse failure as non-fatal. (`src/data/formats/protobuf/proto_decoder.cpp:30`, `ProtoDecoder::decode_file`)
- [ ] A file deleted between discovery and decode can throw through the watcher path. (`src/qt_data/file_service.cpp:55`, `FileService::handleDirectoryChanged`; `src/data/profile_service.cpp:91`, `ProfileService::onFileChanged`)
- [ ] Profile-store saves truncate the destination directly and ignore open/write/serialization failures. (`src/data/formats/protobuf/profile_serializer.cpp:15`, `ProfileSerializer::save`)
- [ ] A protobuf-valid incomplete store can replace the complete store. (`src/data/formats/protobuf/profile_serializer.cpp:47`, `ProfileSerializer::load`; `src/data/profile_service.cpp:97`, `ProfileService::saveProfile`)
- [x] The default KovaaKs directory is read as a schemeless URL and becomes empty. `getKovaaksDirs()` now only round-trips through `QUrl::toLocalFile()` when a value is genuinely stored under the legacy `file/kovaaks` key; the hardcoded default is returned as-is otherwise. Covered by `DefaultKovaaksDirIsReturnedUnchangedWhenUnset`; the stale, tautological `ReturnsDefaultWhenUnset` test was removed. (`src/qt_data/settings_service.cpp:44`, `SettingsService::getKovaaksDirs`)
- [ ] File discovery and protobuf decoding do not preserve Unicode and long paths. (`src/qt_data/file_service.cpp:65`, `FileService::getLatestPerf`; `src/data/formats/protobuf/proto_decoder.cpp:30`, `ProtoDecoder::decode_file`)
- [ ] Directory-creation failures can escape construction and settings callbacks. (`src/data/profile_service.cpp:78`, `ProfileService::ensureParentDir`; `src/data/profile_service.cpp:85`, `ProfileService::applyProfilePath`)
- [ ] Changing the KovaaKs directories repoints the watcher without reloading or reconciling the profile. (`src/qt_data/file_service.cpp:18`, `FileService::FileService`; `src/data/profile_service.cpp:22`, `ProfileService::ProfileService`)
- [ ] Directory scans decode every file instead of filtering to `.perf`. (`src/qt_data/file_service.cpp:23`, `FileService::watchPerfDir`; `src/qt_data/file_service.cpp:39`, `FileService::handleDirectoryChanged`)
- [ ] Watcher add/remove failures are ignored, and a failed repoint can retain known-file state. (`src/qt_data/file_service.cpp:21`, `FileService::repointWatcher`; `src/qt_data/file_service.cpp:30`, `FileService::watchPerfDir`)
- [ ] Store schema upgrades and downgrades rely only on a manual version constant. (`src/data/formats/protobuf/profile_serializer.cpp:19`, `kStoreVersion`; `src/data/formats/protobuf/profile_serializer.cpp:121`, `ProfileSerializer::load`)

## Application and presentation

- [ ] Run-summary generation copies complete `ScenarioPerf` sample vectors before reducing them. (`src/app/session_controller.cpp:125`, `SessionController::getRunsForScenario`)
- [ ] A profile update unconditionally replaces the user's selected run with the latest run. (`src/app/session_controller.cpp:27`, `SessionController::SessionController`)
- [x] `GraphUseCase::load_perf()` passes `string_view::data()` to a length-owning string API. Now constructs `std::string(filename)` explicitly instead of passing `filename.data()`; covered by `LoadPerfPassesOnlyTheGivenViewLengthNotTheWholeUnderlyingBuffer`. (`src/app/usecases/graph_use_case.h:19`, `GraphUseCase::load_perf`)
- [ ] Profile-service callbacks have no unsubscription mechanism and capture raw `this`. (`src/data/profile_service.h:63`, `ProfileService::onProfileChanged`; `src/app/app.cpp:46`, `App::App`)
- [ ] The current-perf callback uses the session controller as context while touching graph-owner state. (`src/app/usecases/graph_use_case.h:34`, `GraphUseCase::onCurrentPerfChanged`)
- [ ] The session-controller fake ignores `getMostRecentPerfs` count and shares load/build observability. (`tests/app/session_controller_test.cpp:48`, `FakeProfileService::getMostRecentPerfs`)
- [ ] `onCurrentPerfChanged` behaviour is covered only by a no-op test double. (`tests/ui/graph_vm_test.cpp:27`, `FakeGraphUseCase::onCurrentPerfChanged`)
- [x] `GraphUseCase` retains a console diagnostic. Removed while fixing the adjacent `string_view` bug above. (`src/app/usecases/graph_use_case.h:19`, `GraphUseCase::load_perf`)
- [ ] `FileService` retains a temporary watcher diagnostic loop. (`src/qt_data/file_service.cpp:42`, `FileService::handleDirectoryChanged`)
- [ ] Refreshing either list model performs a full model reset rather than scoped updates. (`src/ui/presentation/scenario_list_model.cpp:42`, `ScenarioListModel::setSummaries`; `src/ui/presentation/run_list_model.cpp:54`, `RunListModel::setRuns`)
- [ ] `activateScenario()` writes the active name before checking whether the hash changed. (`src/ui/presentation/scenario_browser_vm.cpp:72`, `ScenarioBrowserViewModel::activateScenario`)
- [ ] `runAt()` returns a pointer into a replaceable vector. (`src/ui/presentation/run_list_model.cpp:49`, `RunListModel::runAt`)
- [ ] QML can request `ScenarioPerf` by value without a declared metatype contract. (`src/ui/presentation/session_vm.h:29`, `SessionViewModel::getCurrentPerf`)
- [ ] `GraphCanvas` retains a bare view-model pointer without a destruction guard. (`src/ui/components/graph_canvas.h:77`, `GraphCanvas::m_graphVm`; `src/ui/components/graph_canvas.cpp:32`, `GraphCanvas::setGraphVm`)
- [ ] `axisBounds()` reports raw-space bounds while series are displayed in transformed space. (`src/ui/presentation/graph_vm.cpp:67`, `GraphViewModel::axisBounds`)
- [ ] `forRange()` accepts non-finite ranges and has no upper cap on derived tick counts. (`src/ui/presentation/axis_model.cpp:50`, `AxisModel::forRange`)

## QML performance and state

- [ ] Pointer movement performs repeated linear sampling and by-value series copies. (`src/ui/qml/GraphCanvasWithTooltip.qml:26`; `src/ui/components/graph_canvas.cpp:168`, `GraphCanvas::valuesAtX`; `src/ui/presentation/series_model.cpp:10`, `SeriesModel::sampleAtX`)
- [ ] Tooltip series are rebuilt from a fresh model on every hover event. (`src/ui/qml/GraphCanvasWithTooltip.qml:40`; `src/ui/qml/GraphCanvasWithTooltip.qml:83`; `src/ui/components/graph_canvas.cpp:190`, `GraphCanvas::valuesAtX`)
- [ ] Invisible tooltip-position bindings recompute during hover movement. (`src/ui/qml/GraphCanvasWithTooltip.qml:55`)
- [ ] Every search keystroke filters and resets the scenario model without throttling. (`src/ui/qml/ScenarioSearchPanel.qml:27`; `src/ui/presentation/scenario_browser_vm.cpp:32`, `ScenarioBrowserViewModel::setSearchText`)
- [ ] Column visibility relies on unchecked checked-binding/write-back lockstep across separate definitions. (`src/ui/qml/SettingsDialog.qml:231`; `src/ui/qml/Main.qml:25`; `src/ui/presentation/graph_vm.h:29`, `GraphViewModel::Column`)

## Architecture and project hygiene

- [ ] Layer boundaries are unenforced, with direct cross-layer includes. (`CMakeLists.txt:39`; `src/ui/presentation/settings_vm.h:10`; `src/qt_data/file_service.h:17`)
- [ ] The UI drives `IProfileService` directly. (`src/ui/presentation/settings_vm.h:43`; `src/ui/presentation/settings_vm.cpp:8`, `SettingsViewModel::SettingsViewModel`)
- [ ] The domain layer performs console I/O. (`src/domain/scenario_perf.h:120`, `ScenarioPerf::print`; `src/domain/user_profile.cpp:17`, `UserProfile::addScenarioPerf`)
- [ ] Integration tests use divergent manual QML registration. (`tests/integration/qt_test_main.cpp:16`, `registerQmlTypes`; `src/qml_registration.h:15`, `declare_metatypes`)
- [ ] Release packaging leaves gallery construction enabled. (`CMakeLists.txt:99`; `scripts/package-release.ps1:137`)
- [ ] `ui_qml_tests` lacks the MinGW runtime-DLL copy step. (`tests/ui/CMakeLists.txt:35`)
- [ ] The integration suite contains a view-model stub despite its “no fakes” rule. (`tests/integration/graph_canvas_geometry_test.cpp:93`, `NoYAxisVm`)
- [ ] Commented-out code lacks the required dated TODO marker. (`CMakeLists.txt:109`; `src/ui/qml/AppMenuBar.qml:29`)
