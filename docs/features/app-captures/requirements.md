---
status: approved
---
# App captures

## Summary and user outcome

Let users share a clear, useful image of an individual graph or the benchmark tracking table with minimal effort. A camera action copies a ready-to-share image directly to the clipboard. An adjacent **Export image…** action opens a preview for adjusting the image, copying it, or saving it as PNG.

Captures are clean renditions of the selected content, rendered afresh at the requested output resolution. They preserve the information and context needed to understand the result outside the app, while allowing presentation changes that improve readability. Matching the live widget's exact arrangement is secondary.

The primary outcome is effortless sharing through the user's existing conversation or publishing app. Saving personal snapshots is a secondary benefit. Convenience and a smooth UX govern the feature; increased sharing or engagement is a possible side effect, not a success criterion or a reason to add friction.

## Problem, context, and evidence

The project owner describes sharing today as a multi-step capture-and-crop job that produces an image of disappointing quality. Making a complete result easy to copy removes that work. A visible, local capture action also makes the capability discoverable without requiring users to seek out export functionality.

The [widget image export design](../../../.designs/2026-11-01/widget-image-export/final/design-report.md) provides the visual baseline: camera and dropdown entry points, export preview, resolution controls, an optional benchmark summary, and status-bar feedback. Its [foundation evidence](../../../.designs/2026-11-01/widget-image-export/resources/foundation/foundation-report.md) shows real graph and benchmark layouts, including vertical and horizontal table overflow. Those captures establish the viewport constraints that an export must overcome.

The design study is a presentation prototype. Its preview uses sample bitmaps; it does not demonstrate actual high-resolution export quality, clipboard operations, or save behavior. The product decisions recorded here refine that baseline, particularly layout flexibility, table sizing, identifying context, and completion behavior. This PRD governs those behaviors where it differs from the study.

Problem evidence and product choices come from the project owner's experience and this requirements discussion. They are not findings from broader user research. Rendering quality, practical output limits, and the expected usability improvement still require validation.

## Outcomes and scope

- **O1 — Share with little effort.** Copy a useful image from the relevant widget and paste it into another app without manual cropping or image editing.
- **O2 — Communicate an understandable result.** Retain the selected information and enough identifying context for a recipient to interpret the image on its own.
- **O3 — Produce readable images.** Render at the requested resolution and adapt the export presentation when that improves clarity.
- **O4 — Keep the workflow predictable.** Make preview, sizing, completion, cancellation, and recovery clear while keeping the app responsive.

Scope includes every existing individual graph type: the run graph, daily playtime, scenario history, benchmark personal-best average-rank history, benchmark recent-average rank history, and benchmark rolling playtime. It also includes the complete benchmark tracking table, with an optional associated rank/progress summary. The [graph configuration requirements](../graph-configuration/requirements.md) identify the graph types and their distinct data ranges and presentation settings.

The feature does not include arbitrary screen-region selection, a whole-window capture workflow, a general multi-widget composer, the benchmark manager's editing table, direct uploads or service integrations, an image library, or a new capture settings page. The benchmark summary accompanying its table is the specific exception to capturing one widget at a time. Sharing prompts, engagement reminders, and additional steps intended to increase feature use are outside scope.

## Main journeys

### Copy a result into a conversation

The user views a graph or benchmark, activates its camera action, and receives confirmation when its clean rendition has been copied. The normal path requires one app action and no preview, file dialog, or prior configuration. The user pastes the image into their chosen destination.

### Adjust and share or save an image

The user chooses **Export image…** beside the target widget. The preview presents a fixed snapshot of that content. The user can adjust resolution, change a graph's output dimensions, or include or omit the benchmark summary. The preview and reported dimensions describe the image that Copy or Export will produce.

Successful Copy or Export closes the preview and confirms completion in the main app. Canceling the file picker returns to the unchanged preview. A failure leaves the user's capture choices available for correction and retry.

## Observable requirements

Requirement IDs are stable within this document. Acceptance checks refer to these IDs.

### Entry points and quick copy

Trace: O1, O4.

- **ENTRY-01 — Local, discoverable actions.** Place a camera action beside each supported widget's settings action, following the design baseline. Give it a tooltip and accessible name that identify both copying and the target. Clicking the camera copies; it does not open the preview.
- **ENTRY-02 — Export entry point.** The camera's adjacent dropdown contains **Export image…**, opening the preview for that widget. Keep the direct copy path available without opening this menu.
- **ENTRY-03 — Useful defaults.** Quick copy uses the same clean rendering and content rules as preview export, at 200% resolution. Benchmark quick copy includes the summary by default. It requires no prior visit to the preview or settings.
- **ENTRY-04 — Completion feedback.** Confirm successful copying in the main window's bottom status bar, naming the target. Follow the design's transient success convention of approximately four seconds. Report failures distinctly and accessibly, with a dismiss affordance. Capture feedback must coexist with import progress and warnings without concealing an unresolved warning.

### Content and presentation

Trace: O2, O3.

- **CONTENT-01 — Selected information.** Capture the target's state when the action starts. Graphs retain their current data range, visible series, values, units, and meaningful presentation choices, such as line styles and reference levels. Repositioning elements or adjusting spacing must not silently change the selected data, range, series, or interpretation.
- **CONTENT-02 — Shareable presentation.** Exclude settings and capture buttons, interactive series controls, scrollbars, hover tooltips, and other transient interaction overlays. Replace interactive legend controls with a passive legend identifying the included series. Layout may change to improve readability, including moving the legend, allocating more room to labels, or adapting table columns. Reproducing the live widget's exact arrangement is not required.
- **CONTENT-03 — Graph context.** Include the scenario or benchmark name where applicable, graph type, relevant run identity/date or displayed time range, axis labels and units, and a legend for included series. Context must remain appropriate to the graph: an elapsed-time run graph and a date-based history graph need different range information. Provide this context even if it was supplied by surrounding app UI rather than the live widget itself.
- **CONTENT-04 — Complete benchmark table.** Include every row and column of the selected tracking table, with column headings, identifying labels, displayed values, group structure, and meaningful rank/progress presentation. Capture is independent of live vertical or horizontal scroll position. Pinned identity columns appear once in the exported table. Layout may expand or reflow to keep the content readable; fitting the live viewport is not a constraint.
- **CONTENT-05 — Optional summary, persistent identity.** Include the benchmark name in every table capture. The preview's summary option controls the associated rank/progress summary, not the identifying title or table headings. Start with the summary included. Update the preview and output dimensions when the option changes.
- **CONTENT-06 — Stable snapshot.** Opening the preview fixes the source content and relevant presentation state for that preview visit. New imports, date changes, or underlying data updates must not silently replace the reviewed result. Sizing and summary adjustments affect this snapshot. Reopening the preview obtains a new snapshot. This preserves the source state for rendering; it does not require retaining an earlier rendered image when the current choices cannot render.

### Rendering and sizing

Trace: O2–O4.

- **IMAGE-01 — Fresh rendering.** Render text, graph lines, and other scalable interface elements at the requested output resolution. Enlarging a bitmap of the live widget is insufficient. Higher-resolution output must contain corresponding detail in those elements. Both quick copy and preview output follow this rule.
- **IMAGE-02 — Resolution control.** Start each capture at 200%. Retain the design baseline's 50–500% resolution range and labeled marks at 50%, 100%, 200%, 300%, 400%, and 500%, subject to explicit practical image limits. For an unchanged layout, 200% doubles both pixel dimensions relative to its 100% reference rendering. Resolution changes pixel density rather than revealing additional graph data.
- **IMAGE-03 — Reference and dimensions.** Establish a stable reference layout for each capture rather than recalculating it from the fitted preview's on-screen size. Show the selected resolution and resulting output dimensions in pixels. Report the actual dimensions that Copy or Export will produce. A dialog resize must not silently change output dimensions.
- **IMAGE-04 — Graph sizing.** Advanced options, collapsed initially, allow explicit graph width and height in pixels. Different aspect ratios may cause the layout to rearrange while retaining the selected information and required context. Render into the chosen dimensions without distorting text or stretching an existing image. Dimension and resolution controls must describe one consistent output and make custom sizing understandable.
- **IMAGE-05 — Content-led table sizing.** Derive table dimensions from complete content, its readable layout, summary inclusion, and selected resolution. Table height is content-derived; do not offer an independent fixed-height setting that would force content to be cut or shrunk to fit. The initial table workflow uses resolution to control output pixel density rather than the graph's arbitrary width-and-height controls.
- **IMAGE-06 — Readable, faithful output.** Required labels, context, and values must not be silently cropped or omitted to satisfy dimensions. Wrapping and layout changes are allowed. If requested dimensions cannot accommodate a supported rendition, give actionable validation rather than producing a misleading image. Scaling an oversized preview to fit the dialog must not reduce the resolution of the copied or saved output.

### Preview, copying, and saving

Trace: O1, O3, O4.

- **FLOW-01 — Reviewable preview.** Show the selected target, its rendered content, output dimensions, resolution control, and applicable options. Update the preview when options change. If the current dimensions or content cannot render, clear the rendered image and show an explanatory unavailable/error state. Disable Copy and Export until the current choices successfully render. Keep options available for correction and retry, and restore the preview after successful rendering. Support long or wide table previews without omitting output content, and keep the action footer reachable at constrained window sizes. Preview navigation must not alter the captured content.
- **FLOW-02 — Actions.** Provide Close, a secondary Copy action, and primary **Export…**. Follow the baseline's icon-only Copy with tooltip and accessible name **Copy image to clipboard**. Close, the window close action, and Escape dismiss the preview without copying or saving when no output operation is in progress.
- **FLOW-03 — Clipboard image.** Copy puts the rendered image on the system clipboard in a form that can be pasted as an image into compatible applications. Success means the clipboard operation completed, not merely that rendering began.
- **FLOW-04 — PNG save.** Export opens the normal file-save dialog for a PNG image. The user selects the destination and can cancel or decline replacement of an existing file. Confirm success only after saving completes. The saved image matches the reviewed content and displayed output dimensions.
- **FLOW-05 — Finish or recover.** Successful Copy or Export closes the preview, restores focus to the originating app context, and confirms completion. Canceling the file dialog preserves the preview and all choices. Copy, render, or save failure preserves the source snapshot and choices, shows feedback where the user can perceive it, and allows correction or retry. Render failure uses the unavailable preview state in FLOW-01; clipboard or save failure may retain the successfully rendered current image. Closing the preview by itself performs no output operation.
- **FLOW-06 — Local choices.** Preview changes apply to that capture only. Reopening starts with the default resolution and summary inclusion; preview adjustments do not silently change quick-copy defaults or the live widget's configuration.

### Responsiveness, accessibility, and data handling

Trace: O1, O4.

- **QUALITY-01 — Responsive work.** Keep the app responsive during rendering, clipboard work, and saving. When completion is not immediate, make the in-progress state understandable and prevent accidental duplicate operations. Never report success while an operation is still pending. Do not add artificial delays or unnecessary confirmations to the normal copy path.
- **QUALITY-02 — Accessible interaction.** Support keyboard access to capture actions, preview controls, and completion/recovery actions. Preserve visible focus, meaningful accessible names, and feedback that does not rely on color alone. Controls and the action footer remain reachable when the preview must scroll or reflow at supported text scales and window sizes.
- **QUALITY-03 — Local output and preserved source.** This feature writes only to the system clipboard or a user-selected local destination; it performs no service upload. Images intentionally include the selected result and required identifying context. Capturing must not modify runs, profile data, graph settings, benchmark definitions, or live scroll position, and must not expose unrelated app panels or filesystem paths.

## Failure and edge behavior

Trace: O2–O4.

- **No usable content.** Do not produce a blank or misleading success image when no suitable run, graph data, or benchmark table is available. Make the action unavailable with an understandable explanation, or explain the unavailable content when invoked. If the preview's source content cannot render, show the unavailable state and prevent output. Live data changes do not replace the fixed source snapshot described in CONTENT-06.
- **Invalid dimensions.** Reject nonpositive, malformed, or unsupported dimensions with feedback associated with the input. Clear the rendered preview, show why it is unavailable, and retain the entered choices for correction. Disable Copy and Export until valid choices successfully render; do not fall back to an earlier preview image.
- **Large captures.** Validate supported output sizes before committing to rendering where possible. If a complete table or requested graph output exceeds practical limits, explain that the image is too large and direct the user to reduce resolution or dimensions. Never silently drop rows, columns, context, or series, reduce the requested resolution, or split the result into multiple images. If even the minimum offered resolution cannot produce a supported complete capture, explain that limitation explicitly.
- **Render or resource failure.** Keep the app usable and preserve choices for retry. In the preview dialog, clear the rendered image, explain the unavailable state, and disable Copy and Export until rendering succeeds. Do not substitute an earlier preview or an enlarged low-resolution screenshot. A quick-copy failure leaves the user in the main app with feedback and access to the export preview for adjustment.
- **Clipboard failure.** Do not claim success or deliberately clear the existing clipboard content on failure. In the preview, retain the image and settings for retry.
- **Save failure.** Explain the failure and retain the preview so the user can retry or choose another destination. Do not report a partial write as success or leave a failed replacement represented as a successful image.
- **Cancellation.** File-picker cancellation and declining overwrite are ordinary returns to the preview, without a failure notice. A pending operation has a clear state; dismissal or cancellation must not result in a hidden output action whose completion is falsely reported or lost.

## Constraints, dependencies, and risks

- The target is the existing Windows desktop application and its supported graph/table types. Receiving applications control how pasted images are displayed; the feature can guarantee its image content and dimensions, not a chat application's thumbnail size or compression.
- Rendering quality at the selected resolution is part of the first useful feature, including quick copy. The design study does not prove feasibility or fidelity. The later technical design must establish practical limits and verify that the actual graph and table content can be rendered with the required detail.
- A complete table can produce a very wide or tall image. Fresh rendering improves detail but does not make unlimited content readable in a small recipient-side thumbnail. Completeness takes priority over forcing a viewport-sized image; output limits and failure recovery bound that promise.
- Separate export layouts can drift from the live app. Validation must compare data, units, ranges, rank meaning, and selected presentation state as well as visual quality. Rearrangement is permitted; changed information is not.
- Large images increase memory use and latency. Supported limits must be established with representative content and the supported runtime environment, then surfaced to users. This PRD does not invent a pixel cap or timing target without evidence.
- Current appearance is the default output appearance. A dedicated print theme, transparent background option, image editing tools, and alternate output formats are not required for this scope.

## Evidence of success

Assess the implemented feature primarily through the project owner's use and a focused usability review of the copy, adjustment/save, and recovery journeys. Record that this is limited evidence rather than general user validation.

Look for these observable improvements:

- Copying and pasting a graph or full benchmark result feels simpler than manually capturing and cropping it; the user does not need to prepare the live viewport or edit the resulting image.
- The camera's purpose is understandable, and the direct path remains effortless after discovery. Menus, preview controls, and confirmations do not obstruct routine sharing.
- The resulting image is useful without repair: recipients have enough context to interpret it, required content is complete, and text and graphics are satisfactory at the intended viewing size.
- Adjusting, canceling, and retrying behaves predictably, with clear completion and no apparent app freeze or lost choices.

These are product-success observations. The acceptance checks below establish that required behavior exists, but passing them alone does not establish that the UX feels smooth. No engagement, share-count, conversion, telemetry, or adoption target is required. Increased sharing may follow from a good experience, but must not be pursued at the expense of that experience.

## Acceptance criteria

- [ ] **AC01 — Supported targets and direct copy.** Every graph type in scope and the benchmark tracking table expose the camera and Export image entry points. A single camera activation produces a pasteable image at the default 200% without opening a preview or file picker. Check naming and keyboard access. (ENTRY-01–03, FLOW-03, QUALITY-02)
- [ ] **AC02 — Graph fidelity and context.** For representative run, scenario, playtime, and benchmark history graphs, compare captured data/range, visible series, styles, reference meanings, required labels, and context with the selected source state. Verify that interactive controls and transient overlays are absent and the passive legend is correct. (CONTENT-01–03, IMAGE-06)
- [ ] **AC03 — Complete table.** Capture a benchmark that overflows the live viewport vertically and horizontally from different scroll positions. Each result contains all rows and columns, correct values and groupings, headings, and one copy of each identifying column. It does not depend on the live scroll position. (CONTENT-04, QUALITY-03)
- [ ] **AC04 — Summary choice and identity.** The table summary is initially included. Toggling it updates preview and dimensions; omitting it retains the benchmark title and table headings. (CONTENT-05, IMAGE-05)
- [ ] **AC05 — Resolution and detail.** Compare the same layout at 100%, 200%, and supported higher resolution. Confirm corresponding pixel dimensions and freshly rendered text/line detail rather than bitmap enlargement. Exercise the offered 50–500% range within supported limits. (IMAGE-01–03)
- [ ] **AC06 — Graph dimensions and table sizing.** Change graph width and height, including aspect ratio, and confirm exact reported output dimensions, retained information, and undistorted rendering. Confirm table height follows complete content and table output has no independent fixed-height control. After a valid preview, enter invalid or unsupported dimensions: the image is cleared, an explanation appears, and Copy and Export are disabled. Correct the dimensions and confirm successful rendering restores the preview and output actions. (IMAGE-04–06, FLOW-01, failure and edge behavior)
- [ ] **AC07 — Stable preview and matching output.** Open a preview, then update the underlying data or applicable date context. The preview retains its original snapshot. Change export options and verify clipboard and PNG output match the reviewed content and reported dimensions. Resize the dialog and confirm output dimensions stay fixed. (CONTENT-06, IMAGE-03, FLOW-01, FLOW-03–04)
- [ ] **AC08 — Completion and cancellation.** Successful preview Copy and Export close the preview and confirm completion in the app. Close/Escape perform no output while idle. Canceling a file picker or declining replacement retains the unchanged preview without an error. (ENTRY-04, FLOW-02, FLOW-04–05)
- [ ] **AC09 — Failure and retry.** Exercise clipboard, rendering, oversized-output, and save failures. Feedback is perceptible in the current context, never claims success, and preserves applicable source snapshot/options for retry. Unrenderable content and oversized or failed renders clear the image and disable Copy and Export without showing an earlier preview. A successful retry restores the preview and output actions. Clipboard and save failures retain a valid current preview for retry. Oversized output is not silently reduced, truncated, or split. Quick-copy failure provides a usable path to adjust via the preview. (FLOW-01, FLOW-05, QUALITY-01, failure and edge behavior)
- [ ] **AC10 — Responsive and accessible workflow.** During a representative large supported render, the UI remains responsive, pending work is clear, and duplicate actions do not create unintended outputs. Exercise keyboard navigation, accessible naming, focus return, constrained preview sizes, and supported text scaling. (FLOW-01–02, FLOW-05, QUALITY-01–02)
- [ ] **AC11 — Defaults and source preservation.** Preview changes do not alter the live widget, its scroll position/configuration, or quick-copy defaults. Reopening resets capture options as specified. Capture performs no upload or unrelated data change. (FLOW-06, QUALITY-03)
- [ ] **AC12 — Honest feedback.** Confirm success is reported only after clipboard/save completion; failures remain actionable and distinguishable without color alone. Exercise capture feedback alongside import progress and unresolved warnings. (ENTRY-04, FLOW-03–05, QUALITY-01–02)

## Bounded defaults and non-material open questions

The owner confirmed these defaults before synthesis: PNG using the app's current appearance, quick copy at 200%, per-capture preview adjustments, benchmark summary initially included, and a stable preview snapshot. The design supplies the initial 50–500% slider range and visual direction; arbitrary fixture dimensions and exact button geometry are not output requirements.

Small interaction details remain for later design work: suggested PNG filenames, file-picker starting directory, exact context wording/date formatting, dimension rounding, and how resolution/custom-dimension synchronization is displayed after a graph changes aspect ratio. These must uphold a single understandable output size and the content guarantees above. There is no required capture-preference persistence or settings-page expansion.

Practical pixel/resource limits and rendering latency remain unmeasured technical risks, not validated capabilities. Their values must be established before accepting rendering and recovery behavior. They do not authorize weakening complete-content or fresh-rendering requirements; a finding that normal target content cannot meet those requirements would require revisiting the product scope.
