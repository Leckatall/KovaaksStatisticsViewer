---
type: internal
area: Benchmarks
---
`tst_BenchmarkPicker.qml` — fixed intermittent failures of `test_invalidAndUnsupportedRowsAreNotSelectable` ("window not shown" / null row).
Mechanism: `popup.opened` (and a search-text change) fire before the popup's `ColumnLayout` relayouts for the new `ListView` content. The popup was still sized for an empty list (97px, not ~215px), so the "Manage Benchmarks…" `ItemDelegate`, declared last and therefore on top, still sat over the first entry rows. A click on `bad.json` landed on Manage, which closed the popup and took its items out of the window, so the next `mouseClick` threw "window not shown". When run in isolation the delegates did not exist yet at all, so `row()` returned null.
`openPopup()` and `setSearch()` now call `waitForSettledLayout()`, which waits for the expected rows and for the list/Manage/popup geometry to agree; `row()` resolves only visible, laid-out rows.
