# CHANGELOG.md format

The shape a version section takes in [CHANGELOG.md](../../CHANGELOG.md), assembled from the `changelog.d/` fragments described in [changelog-fragment.md](changelog-fragment.md).

**This is the format reference, not a description of the sections already in CHANGELOG.md.** History stays in whatever format it was written in — changing this template never means reformatting past entries.

**Reading the notation.** Example content is inside fenced blocks; `<angle brackets>` are placeholders. Rules are blockquote lines beginning `> **RULE** —`, sitting immediately below the shape they govern. A `> **RULE**` line is never content: it describes the output, it is never emitted into it.

## Shape

````markdown
## v<version>

### <area>

**Added**
- <the change in one bullet, plus any forward-looking design rationale>

**Fixed**
- <the change + before→after behavior contrast; no old-bug mechanism>
````

> **RULE** — Each fragment becomes **one bullet** stating the change. **Summarize the fragment body — do not copy it.** CHANGELOG.md is a scannable digest, not a transcript of the fragment.
> - **Keep**: forward-looking design rationale (why it is built this way, what it enables), and — for fixes — the before→after behavior contrast, meaning *observable behavior before vs. after*, not the internal code mechanism.
> - **Drop**: the backward-looking mechanism of the old bug — which call, path or field was wrong and why it failed. That detail survives in git via the committed-then-deleted fragment and its diff, so it is not lost by omitting it here.
> - Repeated assembly runs need only converge on the same *shape* — what is included and excluded, and roughly the length — not on identical wording. The printed-to-chat review step backstops the rest.
>
> Example — fragment body:
> ```
> FileService was forwarding the watched directory's own path (from QFileSystemWatcher::directoryChanged) to onFilesChanged instead of the new file's path, so ProfileService::addPerfFileToProfile decoded the directory and failed silently.
> FileService now diffs QDir::entryList snapshots to report the actual new file(s).
> ```
> becomes:
> ```
> - New runs completed while the app is open are now picked up; FileService reports the actual new file(s) rather than the watched directory itself.
> ```
> Kept: the before→after behavior (runs not detected → detected). Dropped: the `directoryChanged` / path-forwarding mechanism — recoverable from the fragment's commit.

> **RULE** — This projection governs future assembly only. Past sections stay in whatever format they were written in (see the intro above); summarizing a fragment never means reformatting history.

> **RULE** — Insertion point: below the intro paragraph, above the previous `## v...` section. Newest first.

> **RULE** — No dates. Not in the version heading, not anywhere in the section.

> **RULE** — One `###` per area, in the order the `area` enum lists them in [changelog-fragment.md](changelog-fragment.md). Omit any area with no fragments.

> **RULE** — Within an area, buckets are `**Added**`, `**Changed**`, `**Fixed**`, `**Removed**`, in that order. Omit any bucket with no fragments.

> **RULE** — `internal` is a fragment type, not a bucket. An `internal` fragment goes under whichever of the four buckets its content fits.

> **RULE** — Merge fragments that say the same thing into one bullet rather than emitting near-duplicates. Reflowing and rewording a fragment body to fit is expected — this is editorial work, not concatenation.

> **RULE** — Every fragment reaches this file, whether or not it carries a `user:` line.
