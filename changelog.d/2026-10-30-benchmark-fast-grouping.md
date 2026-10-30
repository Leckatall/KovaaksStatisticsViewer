---
type: added
area: Benchmarks
user: Group scenarios faster in the Benchmark Manager: drag across cells to select them, press Ctrl+G to create or choose a group, add several categories at once with commas, and keep empty groups and unsorted scenarios visible in the table while you organise.
---
`BenchmarkTableModel` gains `emptyGroupId` rows, so every group has a table row and `BenchmarkEmptyGroupList` is gone, and an `unassigned` flag for direct scenarios in a divided category. `DirectScenarioRelocation`, `addSubcategoryRelocating`, `subcategoryNeedsRelocation` and `BenchmarkEditError::MixedContent` are removed: a mixed draft is allowed and `MixedCategoryContent` keeps it incomplete. New `BenchmarkGroupCommand` popup, backed by `buildGroupCommandOptions`, with `createGroupAndAssign` and `addCategories` on the view model replacing `addCategory`. Range selection moves to one `DragHandler` per `TableView`, on its `contentItem`, so auto-scroll can't end a drag by recycling its delegate. Row moves move to a `rowHandle_<entryId>` grip that shares `DragHandleDots` with the series editor.
