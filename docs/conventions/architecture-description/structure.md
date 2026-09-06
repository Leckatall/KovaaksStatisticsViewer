# Architecture Description Structure

This guide owns deterministic interpretation of the corpus. Read the [global conventions](README.md) for evidence, significance, consistency, and review principles; [root style](style/root.md), [view style](style/views.md), and [diagram style](style/diagrams.md) govern presentation.

## Root, drafts, and managed views

`docs/architecture/README.md` is the only root. It provides system orientation and canonical shared facts, and it links to and summarises views where they are architecturally relevant.

A frontmatter-free Markdown file below `docs/architecture/views/` is a draft document. It is not a managed view and does not participate in inventory, target resolution, reachability, validation, or integration rules.

A managed view starts with exactly:

```yaml
---
id: live-ingestion
name: Live ingestion
---
```

`id` precedes `name`, and no other fields are allowed. The ID is stable lowercase kebab-case, cannot begin with `fact-`, and does not change when the filename, path, title, or display name changes. The first level-one heading equals `name`. An ID change is an explicit migration. Views may be nested anywhere below the views directory and carry no stored parent, child, ownership, or inclusion metadata.

## Managed references

Architectural relationships use explicit full reference links:

```markdown
[Live ingestion details][live-ingestion]
```

Only `[visible text][stable-id]` is a managed use. Shortcut, collapsed, inline, image, frontmatter, code-fence, HTML-comment, and self-reference occurrences do not establish managed relationships.

Each root or managed view has zero or one local registry. When present, it is the final nonblank block:

```markdown
<!-- arch-doc:references:start -->
[live-ingestion]: runtime/live-ingestion.md
<!-- arch-doc:references:end -->
```

Definitions are ordered lexicographically by stable ID and use paths relative to the containing document. Every managed use has a local definition. A definition normally has a use; an intentionally registered-but-unsummarised view is the temporary exception. Registry mutation never authors a use or summary.

Registration means a document's registry contains a view ID. Full integration also requires a managed link and adjacent prose that adequately summarises the view's relevance in that document. The repository tool checks the link and destination; summary adequacy remains semantic judgement.

## Root facts

A claim reused by multiple views may be canonicalised in the root under the reserved `fact-` namespace. Place an invisible anchor immediately before its heading:

```markdown
<a id="fact-profile-authority"></a>
## Profile authority
```

The root registry declares the same ID with `#fact-profile-authority`; consuming views use ordinary managed links whose local definitions are rebased to the root. Fact IDs and anchors are unique and immutable. Replacing a fact means registering a new fact, migrating every consumer, and explicitly removing the unused old registration. The tool locates identities and consumers but never edits fact prose or decides whether a claim deserves canonicalisation.

## Reachability and integration

Reachability is calculated from actual managed reference uses starting at the root. Definitions alone do not make a view reachable, and circular view links are permitted.

The integrated state has every managed view reachable from the root, every managed view and root fact consumed outside its own document, every registry entry used, and every registration accompanied by a link and adequate summary. Unreachable views, unconsumed identities, unused entries, and inadequate summaries are integration gaps. They remain visible warnings and do not become structural failures because temporary incomplete integration is deliberate.

Structural failures are conditions that prevent reliable interpretation or safe scoped mutation: malformed managed frontmatter, duplicate identities, malformed or ambiguous registry delimiters, missing managed definitions or targets, incorrect generated destinations, invalid fact anchors, or invalid ADR structure when ADR validation is selected.

## Maintenance and validation

Use `uv run --frozen python scripts/arch_doc.py` for deterministic inventory, target resolution, reference maintenance, fact-consumer lookup, structural validation, and accepted-ADR recording. The tool never authors architectural prose or decides whether a summary is adequate.

Apply [evidence and accuracy](README.md#evidence-and-accuracy) and [authoring review](README.md#authoring-and-review) at the selected operation's scope. Semantic completeness, summary adequacy, and architectural accuracy remain author judgements. The operation guide controls permitted commands and write authority; this mechanics reference does not authorise mutation.
