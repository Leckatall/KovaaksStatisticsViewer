---
type: changed
area: Benchmarks
user: In the benchmark progress grid, a category without subcategories now spans the Category and Subcategory columns instead of leaving an empty cell beside it.
---
`BenchmarkProgressGrid.qml`: new `spansSubcategory(group)`. A group with `column == "category"` spans both columns when the grid shows the Subcategory column (`hasSubcategories`) and no `subcategory` group starts within the category's rows `[start, start + span)`. That covers scenarios directly under a category and the Uncategorized group; a category with subcategories keeps its own width.
The group cell's width is Category plus Subcategory when spanning. Column measuring skips spanning groups for the Category column and instead widens Category only if the widest spanning name (plus rank chip) would not fit in Category + Subcategory, so the Category column no longer grows for names that have the extra room.
The model (`BenchmarkProgressGridModel`) is unchanged; the rule is derived in QML from the groups list. Covered by `test_categoryWithoutSubcategoriesSpansTheSubcategoryColumn` in `tst_BenchmarkWorkspace.qml`.
