---
status: proposed
---

# Scenario benchmark tracking

## Summary and user outcome

Kovaak's benchmark users need to understand how their performance across a benchmark changes over time, not only inspect isolated scenario scores or a current rank snapshot.

KSV will let users import a Kovaak's playlist or manually create a benchmark, define a shared ladder of score tiers with scenario-specific thresholds, and organize interchangeable scenarios into categories and subcategories. The benchmark tracking view will combine a continuous personal-best average-rank history with official attained and completed ranks, recent performance context, and benchmark-specific playtime.

Benchmark definitions will live as human-readable, KSV-managed files outside the run profile. A Benchmark Manager will make those definitions editable, expose validation failures, and support deliberate refresh after files are copied or edited by hand.

## Problem, context, and evidence

There are currently no tools available to the project owner for displaying benchmark-wide performance over time. Existing score and benchmark experiences can expose individual results or present-day status, but do not reconstruct how normalized performance across the benchmark evolved or show that progression beside the time invested.

The repository includes a representative Kovaak's playlist fixture containing a playlist name and an ordered list of scenario names. It does not contain benchmark tiers, score thresholds, or category structure, so playlist import can seed membership but cannot produce a complete benchmark definition on its own.

KSV already retains run history with scenario identity, score, duration, and time, making retrospective benchmark interpretation possible. This PRD is based on the project owner's direct experience and product decisions; it does not claim broader user research or market validation.

## Goals

- Let users create a benchmark from a downloaded Kovaak's playlist without re-entering its scenario membership.
- Let users create, organize, and maintain their own benchmark definitions entirely within KSV.
- Represent scenario-specific score thresholds on a benchmark-wide, ordered tier ladder.
- Show official attained and completed ranks without losing partial progress or individual-scenario detail.
- Provide a continuous, comparable measure of benchmark performance over calendar time.
- Show recent performance context without allowing it to change permanent personal-best attainment.
- Track benchmark-specific practice intensity and total playtime.
- Keep benchmark definitions independent from the authoritative run profile while remaining inspectable, recoverable, and easy to share between KSV users.
- Surface incomplete, invalid, ambiguous, and unsupported definitions without allowing one problem file to disable the rest of the benchmark library.

## Non-goals

- Predicting how long a user will take to attain the next rank.
- Plotting benchmark progress as a function of cumulative time played. The stored data and metrics may support a later product decision, but this relationship is not part of this feature.
- Defining progress above the highest configured tier. For now, scores at or above the highest threshold are interpreted as that tier for average-rank calculation because no higher interpolation interval exists.
- Making a rank derived from the recent average part of the primary experience. This may be considered later as a low-priority secondary statistic and would never affect attainment.
- Preserving historical versions of benchmark definitions or their former interpretations.
- Continuously watching benchmark files, merging concurrent edits, or maintaining a live relationship with an imported playlist.
- Publishing a stable schema or API for third-party consumers. Human readability and KSV-to-KSV copying do not constitute a compatibility promise to other applications.
- Providing a community catalogue, network download, or external scenario catalogue.
- Defining implementation sequencing, internal architecture, or delivery tickets.

## Users and journeys

### Import and configure a downloaded benchmark

The user selects a playlist JSON from the Kovaak's playlist directory. KSV preserves its scenario order, offers the playlist name as the benchmark name, and creates an incomplete benchmark with every scenario in Uncategorized. The user defines tiers, enters each scenario's thresholds, resolves any ambiguous scenario matches, and optionally organizes interchangeable scenarios before saving a trackable benchmark.

### Create a custom benchmark

The user creates a benchmark in the Benchmark Manager, adds scenarios already known to the profile or adds unplayed scenarios by name, defines the tier ladder and thresholds, and organizes scenarios. Work may be saved while incomplete and resumed later.

### Track benchmark progress

The user selects a complete benchmark and sees its official attained and completed ranks, continuous average-rank history, benchmark-filtered playtime, and the category and scenario detail needed to understand what blocks the next rank.

### Inspect, repair, or share a definition

The user opens the managed benchmark directory, copies a compatible KSV benchmark file into or out of it, or edits a file by hand. Refresh explicitly rescans the directory. The manager loads valid changes and identifies incomplete, invalid, ambiguous, or unsupported files without requiring an application restart.

## Observable requirements

### Benchmark library and file ownership

- Benchmark definitions are stored separately from the run profile in a KSV-managed benchmark directory.
- Each benchmark is represented by its own human-readable, versioned JSON file.
- KSV is the owner and primary editor of these files. Users are not asked to choose or preserve arbitrary file locations.
- The Benchmark Manager lists every loaded benchmark and provides actions to create, import, edit, rename, delete, open the managed directory, and refresh it.
- Files are loaded at application start and when the user requests refresh. KSV does not continuously watch for external changes.
- Copying a compatible benchmark file into the directory and refreshing is a supported KSV-to-KSV sharing workflow. No separate export format is required.
- Refresh is unavailable while the manager contains unsaved edits, preventing an external rescan from silently overwriting them.
- A successful refresh treats the files currently on disk as authoritative: valid additions and modifications load, deleted definitions disappear, and invalid replacements are not silently represented by stale in-memory data.

### Playlist import and manual creation

- Playlist import reads the playlist name when usable and the ordered `scenario_name` entries from the scenario list. Playlist fields unrelated to benchmark definition are ignored.
- A usable playlist name is offered as the initial benchmark name. If none is available, the user supplies a name before the draft can be saved.
- Imported scenario order is preserved.
- The playlist is a one-time seed. The resulting benchmark retains no live dependency on the source file and does not update automatically when that playlist changes.
- Every imported scenario begins in the system-provided Uncategorized section.
- Manual creation permits both selecting a scenario already known to the profile and adding an unplayed scenario by name.
- A scenario may occur at most once within a benchmark, regardless of its position in the hierarchy.
- If a selected playlist lists the same scenario more than once, KSV creates only one membership for that scenario and reports the skipped duplicate entries during import.

### Valid drafts, incomplete benchmarks, and trackable benchmarks

- Imported and manually created benchmarks may be saved before setup is complete.
- A structurally readable definition with unfinished setup is labelled **Incomplete**, not invalid.
- An incomplete benchmark preserves editing progress and identifies every condition preventing completion.
- A benchmark becomes trackable only when it has a non-empty name, at least one scenario, at least one tier, every scenario has a valid threshold for every tier, and it contains no empty user-created category or subcategory.
- Unresolved scenario identity does not make a benchmark incomplete. The scenario remains unplayed until it can be resolved.
- Official ranks and average-rank graphs are unavailable for incomplete benchmarks because partial thresholds would produce misleading results.
- Available playtime and individual-scenario history may still be inspected while a benchmark is incomplete.

### Tier model and validation

- A benchmark defines one ordered tier ladder shared by every scenario.
- Every tier has a non-empty name and an editable color. Tier names are unique within their benchmark.
- Tier order is explicit and editable.
- Every scenario supplies exactly one score threshold for every tier.
- Thresholds strictly increase in tier order. A score equal to a threshold qualifies for that tier.
- KSV assigns sensible default colors, but users may edit them. Colors are not required to be globally unique.
- Tier labels remain visible wherever color is used, so color is never the sole carrier of rank meaning.

### Categories, subcategories, and Uncategorized

- Every user-created category and subcategory has a non-empty editable name and editable color.
- KSV supplies default colors, but uniqueness is not required and textual hierarchy remains visible.
- Uncategorized is a fixed, neutral management section rather than an ordinary category.
- Uncategorized may be empty without making the benchmark incomplete or affecting rank calculations.
- Every scenario left in Uncategorized is an independent rank requirement.
- An ordinary category contains either scenarios directly or subcategories, never both.
- A subcategory is a category nested within another category, and the same satisfaction rules apply to it.
- A category containing scenarios is satisfied for a tier when at least one of those scenarios has attained the tier.
- A category containing subcategories is satisfied for a tier when every subcategory is satisfied.
- Creating the first subcategory in a populated direct-scenario category requires the user to place its existing scenarios into subcategories or return them to Uncategorized.
- An empty user-created category or subcategory makes the benchmark incomplete, so benchmark-wide rank calculations are unavailable until it contains a scenario.

### Scenario identity and resolution

- An imported or manually entered unplayed scenario starts with its scenario name and may exist before the profile contains any matching run.
- If exactly one profile scenario has that name, KSV automatically records its hash mapping.
- If no profile scenario matches, the benchmark entry remains valid and is shown as unplayed and unresolved. It may resolve automatically after matching run data later becomes available.
- If multiple distinct scenario hashes match the name, KSV marks the entry as ambiguous rather than silently combining their results.
- The Benchmark Manager lets the user choose or change the mapping for an ambiguous entry.
- Selecting a known profile scenario during manual creation records its hash mapping immediately.
- Resolved mappings are persisted with the display name retained for recognition and recovery.
- An unresolved scenario participates in rank requirements as unplayed and therefore cannot accidentally satisfy a tier.

### Official rank attainment and completion

- A scenario permanently attains a tier when its personal-best score meets or exceeds that tier's threshold.
- A benchmark attains a tier when every ordinary top-level category is satisfied for that tier and every scenario remaining in Uncategorized has individually attained it.
- The official attained rank is the highest tier satisfying that rule.
- A benchmark completes a tier only when every scenario in the benchmark has individually attained it.
- The tracking view distinguishes, for example, **Gold attained** from **Gold complete** and exposes partial counts and blocking requirements.
- Category organization affects official attainment but does not change scenario weighting in the average-rank metric.

### Continuous average-rank performance

- Tier positions form a numeric scale: the first tier is `1`, the second is `2`, and so on.
- A scenario score between adjacent thresholds is converted to a fractional tier position using linear interpolation. A score halfway between tier 1 and tier 2 contributes `1.5`.
- Progress below the first threshold is represented proportionally between unranked `0` and tier `1`.
- An unplayed scenario contributes `0`.
- For this calculation only, a score at or above the highest threshold is interpreted as the highest tier value. The raw score and personal best remain unchanged.
- The benchmark's average rank is the arithmetic mean of every scenario's converted value. Every scenario has equal weight, irrespective of category structure.
- Overperformance in one scenario may offset weaker performance elsewhere in the average. This is accepted behavior and is why official attained rank remains separately visible.
- The primary performance graph is a continuous line over calendar time. At each point it uses the personal best attained for every scenario by that date, making the line non-decreasing for an unchanged benchmark definition.

### Recent performance

- Every scenario displays a secondary recent average calculated from its latest five completed runs.
- When fewer than five runs exist, the average uses all available runs and indicates the smaller sample where necessary for correct interpretation.
- Recent average may rise or fall and never grants or revokes permanent attainment.
- Personal best and recent average are distinctly labelled and visually distinguishable.

### Benchmark playtime

- Benchmark playtime includes runs from the unique resolved scenarios belonging to the selected benchmark.
- The tracking view displays total accumulated benchmark playtime.
- A line graph displays the existing three-day rolling-average playtime measure filtered to the benchmark's scenarios and plotted over calendar time.
- Unresolved or unplayed scenarios contribute no playtime until their run identity resolves.

### Tracking and management views

- The tracking view and Benchmark Manager are separate product surfaces: tracking explains performance, while management changes its definition.
- A benchmark status summary displays official attained rank, highest completed rank, current average rank, total playtime, and the remaining requirements for the next tier.
- The history area displays the personal-best average-rank line and benchmark-filtered rolling-playtime line.
- The scenario breakdown follows the category hierarchy and shows each scenario's personal best, recent five-run average, attained tier, next threshold, and matching state.
- Rank colors, category colors, names, and hierarchy are used consistently across summary and detail views without relying on color alone.

### Retrospective interpretation after edits

- The current benchmark definition is the authoritative interpretation of all available run history.
- Changing thresholds or membership recalculates past average-rank values and rank attainment under the new definition.
- Adding an unplayed scenario may lower historical average rank; removing a scenario may raise it.
- Changing categories may change official attainment history without changing the average-rank line.
- Structural and threshold edits warn users that historical results will be reinterpreted.
- KSV does not retain or display earlier definition versions in this feature.

## Failure and edge behavior

- A malformed playlist or one without a usable scenario list fails import with a useful explanation and does not create or mutate a draft.
- Cancelling creation, import, or editing before save leaves the last saved library state unchanged.
- An unreadable benchmark file or content KSV cannot safely interpret is shown as **Invalid** in the manager rather than as an incomplete benchmark.
- An invalid entry shows the benchmark name when that field can be parsed safely; otherwise it is identified by filename.
- Validation identifies the specific problem and, where possible, the affected tier, category, subcategory, or scenario.
- One invalid or unsupported file does not prevent other benchmark files from loading.
- A file using a newer unsupported schema version is reported as unsupported and is not rewritten.
- Supported older files may be migrated. A migration failure leaves the original recoverable and reports the failure rather than claiming success.
- Fixing or replacing a problem file and refreshing clears its error when the new content is valid.
- If a selected benchmark is deleted or becomes invalid on refresh, the tracking view reports that it is unavailable instead of continuing to show stale results.
- Save failures are reported and do not replace the last successfully stored definition.
- Multiple same-name profile candidates remain ambiguous until the user resolves them; KSV does not guess based on run count, score, or recency.
- Manual file edits are accepted only after a successful refresh and validation. KSV does not merge them with unsaved manager edits.

## Product-relevant constraints, dependencies, and risks

- The serialized profile remains the authoritative record of runs. Benchmark files only define how those runs are grouped and interpreted.
- Playlist membership is name-based, while run identity is hash-based. The persisted resolution and ambiguity workflow are necessary to support unplayed scenarios without silently attributing scores to the wrong scenario.
- Human-readable files invite manual editing. Explicit refresh, strong validation, recoverable migration, and visible per-file errors are therefore part of the product rather than implementation conveniences.
- The average-rank metric intentionally permits high performance in one scenario to offset low performance in another. Official attained and completed ranks must remain adjacent and clearly distinguished to prevent the average from being mistaken for certification.
- Interpreting scores above the highest tier as the highest tier understates further overperformance. This is a known necessary limitation until a defensible extrapolation rule is defined.
- Retroactive reinterpretation keeps the model understandable and avoids hidden definition history, but edits can visibly reshape past graphs. Warnings and clear current-definition language mitigate surprise.
- Configuring thresholds across a large playlist may require substantial data entry. Incomplete drafts and precise validation are essential to make that work recoverable.
- Names and hierarchy must accompany all uses of configurable colors to preserve accessibility and comprehension.

## Evidence of success

The feature succeeds when a benchmark user can use retained run history to answer, without manually recording or normalizing scores:

- How has my average benchmark rank changed over calendar time?
- What rank have I officially attained, and what rank have I completed?
- Which categories, subcategories, or scenarios prevent the next rank?
- Is my recent performance consistent with my personal-best standing?
- How much and how consistently have I practised this benchmark?

For this personal/open-source desktop product, successful use and trustworthy answers to those questions are the available product evidence. This PRD does not invent adoption targets, deadlines, or telemetry requirements.

## Acceptance criteria

- [ ] Given the representative playlist fixture, importing it preserves its scenario order, offers its playlist name, and creates an incomplete benchmark with every unique scenario in Uncategorized.
- [ ] Given a manual benchmark, the user can add both known profile scenarios and unplayed scenarios by name.
- [ ] Given an incomplete benchmark, saving and reopening it preserves all entered work, identifies remaining setup issues, and does not expose official or average-rank calculations as complete results.
- [ ] Given a benchmark with no name, no scenarios, no tiers, missing thresholds, or an empty user-created category or subcategory, it remains incomplete and cannot present group rank calculations; an empty Uncategorized section does not have this effect.
- [ ] Given a benchmark with shared ordered tiers, every scenario can store a strictly increasing threshold for each tier, and equality with a threshold qualifies for that tier.
- [ ] Given scenarios in Uncategorized, each must individually attain a tier before the benchmark can officially attain it.
- [ ] Given a direct-scenario category, any qualifying scenario satisfies that category; given a category of subcategories, every subcategory requires at least one qualifying scenario.
- [ ] Given a category with direct scenarios, KSV prevents it from simultaneously containing subcategories.
- [ ] Given every scenario in the benchmark has attained a tier, the benchmark reports that tier as complete; if any scenario has not attained it, the benchmark does not report that tier as complete.
- [ ] Given score values between tier thresholds, KSV produces the expected fractional tier values and averages all benchmark scenarios equally, including unplayed scenarios as zero.
- [ ] Given a score above the highest threshold, average-rank calculation interprets it as the highest tier while retaining the uncapped raw score and personal best.
- [ ] Given historical runs, the average-rank graph reconstructs the personal-best average at each date and does not decrease unless the benchmark definition changes.
- [ ] Given more than five runs for a scenario, recent average uses exactly its latest five and does not alter permanent attainment.
- [ ] Given a selected benchmark, the tracking view shows official and completed ranks, average-rank history, total playtime, three-day rolling playtime, and scenario-level blockers.
- [ ] Given zero, one, or multiple name matches in the profile, KSV respectively keeps the scenario unresolved, binds it automatically, or exposes an ambiguity requiring user choice.
- [ ] Given a valid benchmark file copied or edited in the managed directory, refresh loads it without restarting the application.
- [ ] Given an invalid benchmark file, the manager shows its name when safely available, otherwise its filename, plus a useful validation reason while continuing to load valid benchmarks.
- [ ] Given unsaved manager edits, refresh cannot silently replace them.
- [ ] Given threshold, membership, or hierarchy edits, saved tracking results are recalculated across existing history and the user is warned that interpretation will change.
- [ ] Given an unsupported newer file version or failed migration/save, KSV preserves recoverable data, reports the failure, and does not claim that the benchmark loaded or saved successfully.

## Assumptions and non-material open questions

- Benchmark scores use the Kovaak's convention that higher values are better and meaningful scores are non-negative.
- Automatic name matching uses exact names to avoid false attribution; manual resolution remains available when exact matching is insufficient.
- Tier, category, and subcategory colors receive application defaults and may repeat. Exact palettes and color-picker interaction are presentation decisions.
- The tracking view uses all available profile history by default. Graph navigation and optional date-range controls may follow existing KSV graph conventions without changing the product model.
- Benchmark display ordering and filename generation are manager presentation details as long as benchmark identity, content, validation, and recovery behavior remain stable.
