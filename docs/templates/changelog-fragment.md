# Changelog fragment format

**Every change gets a fragment**: a new file `changelog.d/YYYY-MM-DD-short-slug.md`, committed alongside
the change it describes. Writing one requires reading nothing — not the directory, not the changelogs.

```markdown
---
type: added
area: Graphing
user: New playtime graph, tracking play time alongside your performance stats.
---
`PlaytimeGraphViewModel` — new VM on `GraphViewModelBase`, wired in `App::App()`, added to `Main.qml`
initial properties. Shares axis calculation with `GraphViewModel` via `AxisModel`.
```

`type` is one of `added`, `changed`, `fixed`, `removed`, `internal`. `area` is one of
**Scenario & run selection, Graphing, Sessions, Settings, Data & profile cache, Build & packaging,
Architecture** — add a new one only when none fit. The body is developer detail: which classes, which
layers, what a future reader could not recover from the diff.

**`user` is optional, and it is the whole user-facing/internal distinction.** Include it only if someone
using the app would notice the change, and write it in their vocabulary, not the codebase's — it is
copied verbatim into the release notes. Refactors, tests, tooling and build changes omit it entirely.
Nothing is ever written twice: one fragment feeds both outputs.

## Lifecycle

At version bump the fragments are assembled into [CHANGELOG.md](../../CHANGELOG.md) (everything) and
[RELEASE_NOTES.md](../../RELEASE_NOTES.md) (`user` lines only), then deleted — git history is the
archive. The shapes they are assembled into are described alongside this file:
[CHANGELOG.template.md](CHANGELOG.template.md), [RELEASE_NOTES.template.md](RELEASE_NOTES.template.md).

Everything in `changelog.d/` is transient. Deletion is an unconditional `rm changelog.d/*.md`, and the
only survivor is the tracked `.gitkeep` — which survives because `*.md` does not match it, not because
of an exclusion. That is why these format documents live in `docs/templates/` and not in `changelog.d/`:
nothing may be added to that directory that the release is not free to delete.
