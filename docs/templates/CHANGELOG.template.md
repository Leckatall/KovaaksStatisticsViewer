# CHANGELOG.md format

The shape a version section takes in [CHANGELOG.md](../../CHANGELOG.md), assembled from the
`changelog.d/` fragments described in [changelog-fragment.md](changelog-fragment.md).

**This is the format reference, not a description of the sections already in CHANGELOG.md.** History
stays in whatever format it was written in — changing this template never means reformatting past
entries.

**Reading the notation.** Example content is inside fenced blocks; `<angle brackets>` are placeholders.
Rules are blockquote lines beginning `> **RULE** —`, sitting immediately below the shape they govern.
A `> **RULE**` line is never content: it describes the output, it is never emitted into it.

## Shape

````markdown
## v<version>

### <area>

**Added**
- <fragment body, reflowed to ~110 columns>

**Fixed**
- <fragment body>
````

> **RULE** — Insertion point: below the intro paragraph, above the previous `## v...` section. Newest
> first.

> **RULE** — No dates. Not in the version heading, not anywhere in the section.

> **RULE** — One `###` per area, in the order the `area` enum lists them in
> [changelog-fragment.md](changelog-fragment.md). Omit any area with no fragments.

> **RULE** — Within an area, buckets are `**Added**`, `**Changed**`, `**Fixed**`, `**Removed**`, in
> that order. Omit any bucket with no fragments.

> **RULE** — `internal` is a fragment type, not a bucket. An `internal` fragment goes under whichever
> of the four buckets its content fits.

> **RULE** — Merge fragments that say the same thing into one bullet rather than emitting
> near-duplicates. Reflowing and rewording a fragment body to fit is expected — this is editorial work,
> not concatenation.

> **RULE** — Every fragment reaches this file, whether or not it carries a `user:` line.
