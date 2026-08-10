# RELEASE_NOTES.md format

The shape a version section takes in [RELEASE_NOTES.md](../../RELEASE_NOTES.md), assembled from the
`changelog.d/` fragments described in [changelog-fragment.md](changelog-fragment.md).

**This is the format reference, not a description of the sections already in RELEASE_NOTES.md.**
History stays in whatever format it was written in — changing this template never means reformatting
past entries.

**Reading the notation.** Example content is inside fenced blocks; `<angle brackets>` are placeholders.
Rules are blockquote lines beginning `> **RULE** —`, sitting immediately below the shape they govern.
A `> **RULE**` line is never content: it describes the output, it is never emitted into it.

## Shape

````markdown
## v<version>

### Features & changes

#### <area>
- <the fragment's `user:` line, verbatim>

### Bug fixes
- <the fragment's `user:` line, verbatim>
````

> **RULE** — Insertion point: below the intro paragraph, above the previous `## v...` section. Newest
> first. No dates.

> **RULE** — Only fragments carrying a `user:` line appear here. A fragment without one is
> CHANGELOG.md-only.

> **RULE** — The `user:` text is copied **verbatim**. It was written in the user's vocabulary on
> purpose; do not rewrite it, and do not fall back to the fragment body.

> **RULE** — Two `###` sections, in this order: `added`, `changed` and `removed` fragments go under
> `### Features & changes`; `fixed` fragments go under `### Bug fixes`. Omit either section entirely
> when it has no fragments.

> **RULE** — `internal` is a fragment type, not a section. An `internal` fragment carrying a `user:`
> line goes under whichever of the two sections its content fits.

> **RULE** — Inside `### Features & changes`, one `####` per area, in the order the `area` enum lists
> them in [changelog-fragment.md](changelog-fragment.md). Omit any area with no user-facing fragments.

> **RULE** — `Data & profile cache`, `Build & packaging` and `Architecture` collapse into a single
> `#### Under the hood`, placed last within `### Features & changes`. It is a features subheading
> only — it never appears under `### Bug fixes`.

> **RULE** — `### Bug fixes` is one flat list with no area subheadings, ordered by the `area` enum so
> fixes touching the same area sit together.

> **RULE** — If no fragment in the release carries a `user:` line, this file gets no section at all.
> Skip it and say so out loud rather than inventing user-facing text from fragment bodies.
