---
type: internal
area: Architecture
---
Removed tests that pinned defaults, restated the code, or asserted colours and styling, so a non-breaking tweak no longer forces a test edit. Mixed tests keep their behavioural checks; literal copies became relative comparisons. `fake_profile_service_test.cpp` and `gallery_dataset_test.cpp` are gone, along with the `KSV_UI_QML_DIR` define and `FakeFileService::callbackCount()` they left unused.
