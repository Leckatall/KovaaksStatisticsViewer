---
status: proposed
---

# Benchmark table editor

## Summary and user outcome

Replace the Benchmark Manager's card-based definition editor with a compact editable table. Users should be able to transfer the relevant columns from their benchmark spreadsheet, inspect the resulting definition, and repair mistakes in place without entering thresholds one scenario at a time.

Each scenario occupies one row. Each rank occupies one threshold column, with its name and colour control in the header. Category and subcategory names occupy separate columns with vertically merged cells spanning their member rows. Colour controls sit inside the corresponding group cells.

Threshold paste is the primary capability. Scenario names and rank names can be pasted separately to support spreadsheets whose relevant columns are separated by personal scores, progress, or other content. Users create categories and subcategories manually and assign selected scenario rows to them. Table order follows the benchmark hierarchy, keeping each group's scenarios adjacent.

## Problem, context, and evidence

The project owner reports that defining benchmarks takes too long: scenario cards are bulky, repeat rank names beside every scenario's thresholds, and make entered data difficult to inspect. The current editor also includes scenario matching information and management controls within its scenario and group cards.

The reference workbook, [Copy of Viscose Benchmarks Beta (File → Make a copy).xlsx](../../../../tests/examples/Copy%20of%20Viscose%20Benchmarks%20Beta%20(File%20%E2%86%92%20Make%20a%20copy).xlsx), demonstrates the intended layout. On the Easier Scenarios tab, categories and subcategories are merged vertically in columns A–B, scenario names occupy C, and eight rank headers and their thresholds occupy H–O. Other columns contain information irrelevant to defining a benchmark. This supports separate paste operations rather than requiring a whole-sheet import.

The problem evidence is the project owner's direct experience and the supplied workbook. Faster setup and easier inspection are product goals, not measured results or claims of broader user research.

This PRD refines the editing experience defined in the [benchmark requirements](../requirements.md). It does not change rank calculation, hierarchy semantics, scenario identity, or ownership of saved benchmark files.

## Goals and scope

- Reduce the effort of transferring benchmark definitions from existing spreadsheets, especially per-rank threshold columns.
- Make thresholds across scenarios and ranks easy to inspect together, with rank labels shown once in the header.
- Let users paste imperfect data, see exactly where it needs attention, and correct it without restarting the transfer.
- Keep grouping, rank configuration, and colour selection available directly in the table.
- Preserve the existing benchmark creation, playlist import, scenario mapping, incomplete-save, and library-management capabilities.

Whole-sheet import, direct workbook-file import, category/subcategory label paste, inference of grouping from clipboard blanks or merged cells, spreadsheet formula evaluation, automatic discovery of relevant source columns, and copying spreadsheet formatting or colours are outside this feature. Users select the source columns and destination themselves. Persistent drafts containing malformed raw text are also outside scope.

## Main journeys

### Configure a benchmark from a spreadsheet

The user creates a benchmark or starts from an imported playlist. They paste scenario names into the Scenario column if needed and rank names into the rank-header area. They paste one threshold column or several adjacent threshold columns starting at the appropriate destination cell. They create categories and subcategories manually, select the corresponding scenario rows, and assign them to groups. The table keeps each group's members adjacent, with thresholds and mappings following their scenarios. They inspect the compact table, fix flagged cells, choose colours, and save.

No paste requires personal scores, progress columns, workbook titles, or other unrelated sheet content. Users may enter scenario names, rank names, and thresholds manually as well.

### Repair a partial or malformed paste

The user pastes a block containing valid scores, blanks, and malformed values. The entire block appears at its intended coordinates, including malformed text. Issues appear alongside the affected cells. The user edits those cells directly or undoes the whole paste. If they save before repairing wrong-type threshold values, those values become missing thresholds in the saved incomplete definition.

### Maintain an existing benchmark

The user opens a saved benchmark, changes a selected subset of thresholds or grouping, and saves through the existing historical-reinterpretation safeguards. Values outside the pasted area remain intact. Existing scenario mappings and benchmark identity remain governed by the current manager rules.

## Observable requirements

### Table layout and editing

- Present one row per scenario and one column per rank in the benchmark's explicit rank order.
- Present rank names once as editable column headers, with a colour swatch beside each name.
- Present Category, Subcategory, and Scenario columns before the threshold columns. Categories without subcategories and scenarios in Uncategorized remain supported.
- Use a defined hierarchy order: category order, subcategory order within a category, and scenario order within each containing group. Keep all scenarios in a category adjacent and all scenarios in each subcategory adjacent within that category. Uncategorized remains a distinct neutral section.
- Vertically merge category and subcategory cells across their adjacent member rows. The hierarchy and names must remain readable without relying on colour alone.
- Expose a colour swatch inside each user-created category or subcategory cell. Activating a swatch opens colour selection from that location; cancelling leaves the colour unchanged.
- Allow manual cell editing and a clear selected destination for paste. Users must be able to move between editable cells with the keyboard.
- Retain access to adding, renaming, removing, and ordering ranks; managing categories and subcategories; adding, moving, and removing scenarios; and inspecting or changing scenario mappings. These capabilities must not require restoring the card editor.
- Keep mapping details and management actions accessible without permanently expanding every ordinary scenario row into a card.
- Keep entered values, missing values, selected cells, and validation issues visually distinguishable. A malformed value must remain readable and editable alongside its issue.

### Paste placement and separate passes

- Support ordinary clipboard paste of tabular text copied from a spreadsheet, with tabs separating columns and line breaks separating rows.
- Paste begins at the selected destination and fills across and down in clipboard order. It does not search for matching scenario or rank names to choose different destinations.
- Support a single rank's threshold column and a rectangular block spanning multiple rank columns and scenario rows.
- Support scenario-name paste into the Scenario column, creating rows as needed, and rank-name paste into the header area, creating rank columns as needed.
- Support paste into scenario-name cells, rank-name headers, and threshold cells only. Category and subcategory cells do not accept grouping paste.
- Separate paste passes address the displayed scenario rows and rank columns in their current order. Pasting names or thresholds must not regroup or reorder existing scenarios. A manual grouping or ordering action may change visible row positions; subsequent paste starts at the newly selected destination in that visible order.
- Values inside the pasted rectangle replace the corresponding existing cell contents. Cells outside that rectangle remain unchanged, except for table expansion needed to accommodate the paste.
- Blank cells inside a threshold block clear the corresponding thresholds. A numeric zero is an actual value, not a blank or replacement for missing data.
- Preserve internal empty cells and their positions. A routine terminal line break in clipboard text must not create an extra empty scenario row.
- Each paste is one undoable operation. One Undo restores overwritten values, malformed text, and any rows or rank columns created by that paste.

### Automatic expansion

- When pasted data exceeds the available rows or rank columns, expand the table to retain the entire block. Do not truncate or reject it solely because of its dimensions.
- A threshold-only paste may create rows without scenario names and columns without rank names. Show the missing names at those rows or headers so users can supply them afterward.
- Placeholder labels used to explain missing names must not be saved as user-entered scenario or rank names.
- New ranks and groups receive the manager's ordinary default colours until users choose otherwise.

### Manual grouping and merged cells

- Let users create, rename, remove, and order categories and subcategories manually without restoring the card editor.
- Let users select one or more scenario rows and assign them to a category, a subcategory, or Uncategorized.
- Grouping and ordering actions keep each scenario's thresholds, display name, and existing hash mapping attached to that scenario. Move rows into the target group's defined order and update merged spans to reflect membership.
- Renaming a merged group cell changes the group's name; assigning selected rows changes membership. These actions must be distinguishable.
- Preserve the existing hierarchy rules: a subcategory has a parent category, and a category contains either direct scenarios or subcategories. Do not silently reorganize other scenarios to accommodate an incompatible assignment; explain the conflict and let the user choose a valid arrangement.
- Do not infer or create groups from pasted labels, blank cells, or source spreadsheet merge boundaries.

### Validation and repair

- Accept partially malformed paste into the editor. Valid values and malformed text must appear together at their original destinations; one problematic value must not reject the entire block.
- Preserve the supplied text of a wrong-type threshold value while editing. Do not silently parse a numeric prefix, shift subsequent values, substitute zero, or drop the cell.
- Show issues beside the affected cells, rows, headers, or merged groups. Distinguish wrong-type values from missing names, missing thresholds, duplicate names, invalid numeric scores, and hierarchy problems where applicable.
- Update issues after correction. Users must be able to repair pasted text directly, without reopening an import flow or pasting the whole block again.
- Retain existing benchmark validation, including non-negative finite scores, unique non-empty rank names, and thresholds strictly increasing in rank order.
- Unresolved or ambiguous scenario identity remains a separate mapping condition. Pasting display names must not silently select or replace an existing hash mapping.
- Show textual issue information; colour alone must not communicate an error or the meaning of a rank or group.

### Save, incomplete work, and recovery

- Do not persist malformed raw text or expand the saved benchmark's value types to accommodate it.
- Before saving, normalize wrong-type threshold values to missing thresholds using the existing typed representation. Use missing/null rather than zero because zero is a real score.
- Save need not wait for the user to repair every wrong-type threshold value. A benchmark with the resulting missing thresholds can be saved as incomplete under the existing manager rules.
- Make the normalization apparent: after successful Save, affected cells display missing values and the applicable validation issues. Reopening shows missing values, not the original malformed text.
- Normalization must not erase valid neighbouring thresholds or silently repair unrelated numeric, naming, hierarchy, or mapping issues. It does not establish that the benchmark is trackable.
- Preserve the existing requirement for a benchmark name, incomplete/invalid classifications, and exclusion of incomplete definitions from official rank and average-rank results.
- Preserve existing Save/Discard/Cancel protection for unsaved work and the warnings about reinterpretation of historical results when saved definitions are changed.
- Cancelling a transition keeps the current table and malformed text available. Discard restores the saved state. A failed save leaves the stored benchmark unchanged and retains the editable input for retry or repair.
- After a successful save, malformed source text is no longer recoverable by reopening the benchmark. The in-editor Undo capability is not a promise of persistent edit history.

## Product constraints and risks

Saved definitions remain typed benchmark data. Keeping malformed text only in the open editor permits a forgiving transfer workflow without introducing persistent malformed drafts or changing what benchmark evaluation accepts.

Positional paste makes separate source-column copies straightforward, but users can choose the wrong destination. A visible destination, preserved row/column alignment, nearby issue feedback, and one-step paste Undo provide the required recovery. Paste must not guess source-column meaning.

The reference workbook already keeps each group's scenarios adjacent. The editor uses the same hierarchy layout, while grouping is configured manually because clipboard text does not reliably communicate source merge boundaries. A deliberate grouping or ordering action can change row positions, so users must select the destination in the current table order for each subsequent paste. Thresholds and mappings remain attached to their scenarios when rows move.

Changing hierarchy, membership, rank order, or thresholds can reinterpret historical results. This editor preserves the existing warning and calculation behavior; spreadsheet convenience must not bypass it.

## Evidence of success

Use the supplied workbook for a practical comparison with the card editor: configure a benchmark from one scenario tab by copying only the desired columns, then inspect and repair the result.

The product succeeds when the project owner can transfer threshold columns without per-scenario re-entry, review scores across ranks without repeated labels or bulky cards, and locate and repair intentionally malformed pasted cells without repeating the transfer. Setup effort and inspection clarity should improve over the card workflow. A timed comparison may support that judgment, but no numerical target, wider user validation, telemetry requirement, or measured improvement is claimed here.

## Acceptance criteria

- [ ] A benchmark displays one scenario per row, shared editable rank headers in rank order, and Category/Subcategory cells merged across adjacent member rows in the benchmark's defined hierarchy order.
- [ ] A rank-header swatch and a group-cell swatch allow colour changes from their table locations; cancellation preserves the prior colour and names remain visible.
- [x] Pasting one threshold column into a selected rank cell fills the intended scenario rows without changing neighbouring rank columns.
- [x] Pasting a multi-rank threshold block fills across and down without requiring unrelated source columns or matching by name.
- [x] Separate scenario-name, rank-name, and threshold pastes can build a definition without regrouping or reordering existing scenarios; every paste uses the selected destination in the current visible order.
- [x] A pasted blank clears an existing threshold, zero remains zero, internal blanks preserve alignment, and a terminal line break creates no extra row.
- [x] An oversized threshold block is fully retained in expanded rows/columns, with missing scenario and rank names visibly flagged.
- [x] One Undo reverses a paste including overwrites, raw malformed text, and row/rank-column expansion.
- [x] Users can create groups manually and assign selected scenario rows to a category, subcategory, or Uncategorized; group members remain adjacent and merged spans update.
- [x] Grouping or ordering scenarios preserves their thresholds and existing mappings; subsequent paste targets the current visible row order.
- [x] Category and subcategory cells do not accept grouping paste, and clipboard blanks or merged-cell labels do not create groups or change membership.
- [x] An incompatible manual grouping assignment is explained without silently changing the hierarchy or its rank meaning.
- [x] A block containing valid scores, blanks, and a value such as `oops` appears in full, with `oops` preserved and flagged at its original cell; editing it to a valid score updates the issue.
- [x] Negative, non-finite, or non-increasing numeric scores and duplicate or missing names receive the applicable validation feedback without being mistaken for valid trackable configuration.
- [x] Saving a named benchmark containing wrong-type threshold text stores missing thresholds instead of that text or zero; valid neighbouring values survive and reopening shows the missing values and incomplete state.
- [x] A failed or cancelled save retains the editable malformed input and leaves the saved benchmark unchanged.
- [x] Existing manual management, playlist seeding, scenario mapping, unsaved-work protection, historical-reinterpretation warnings, and incomplete-save behavior remain available.

## Bounded assumptions and presentation questions

- Use clipboard cell values rather than evaluating spreadsheet formulas or reproducing formatting. Exact clipboard-format support and numeric-locale conventions should follow the desktop application's supported input conventions; ambiguous input must remain visible as an issue rather than being guessed.
- The table follows the benchmark's explicit hierarchy and scenario ordering, including after saving and reopening. Arbitrary row order that interleaves members of different categories is not supported.
- Uncategorized remains the existing neutral destination for scenarios without category membership. Group assignment is an explicit manual action.
- Exact cell sizes, sticky headers, frozen identifying columns, placement of row actions and mapping details, and colour-picker presentation are design questions. They must preserve compact inspection and clear paste destinations.
- Undo is required for each paste within the editing session. Persistence of undo history, a general spreadsheet editing engine, and bulk-edit features beyond assigning selected scenario rows to groups are not requirements of this PRD.
