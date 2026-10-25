---
type: fixed
area: Architecture
---
Two dangling references the MinGW build tolerated by luck and MSVC's debug heap exposed as crashes.
`PlaylistReader::read` took `scenarioList[i].toObject()["scenario_name"]`: the non-const `QJsonObject::operator[]` on a temporary object yields a `QJsonValueRef` (deduced by `auto`) into an object destroyed at the end of that statement, so every later `isString()`/`toString()` read freed memory. It now uses `.value("scenario_name")`, which returns a `QJsonValue` by value.
`SettingsViewModel::runAxisLabelChoices` and `runAxisLabelKey` bound `const auto &` into `shownConfiguration()->…`; outside a visit `shownConfiguration()` returns a `shared_ptr` that can be the configuration's only owner, so the referenced document died with the temporary. Both now hold the `shared_ptr` in a local first, as the other callers already did.
Tests with the same lifetime mistake (pointers into the `std::optional` returned by value from `IBenchmarksService::snapshot()`, a reference into the `GraphConfiguration` returned by value from `GraphDraftTest`'s `saved()`) now bind the value to a local; `ProfileServiceTest` passes `UserProfile{}` explicitly, since `UserProfile`'s default constructor is `explicit` and MSVC rejects `setProfile({})`. `from_chars` end checks in `stats_csv_parser.cpp`/`run_filename.cpp` compare against `data() + size()`, not `string_view::end()`, which is only a pointer on libstdc++.
