---
type: internal
area: Documentation
---
Architecture-description conventions now define viewpoints and when a concern needs a view.
`style/views.md` gains a closed set of three viewpoints (`structural`, `runtime`, `information`), each with its questions, elements, relationship meaning and lead models; combined views; a per-viewpoint modelling section (the old scenario guidance becomes the runtime one); parent views that must carry their own architectural account; and a fictional information-view worked example alongside the runtime one.
`style/root.md` replaces the permissive extraction triggers with a summary-depth limit for every non-orientation concern and a maintainer-task test for when a concern needs a view, applied when authoring rather than only by later extraction. Length is no longer a reason to keep detail in the root.
Managed view frontmatter gains a required `viewpoints` list after `id` and `name`. `scripts/arch_doc.py` validates it as a non-empty list of distinct known values (flow or block style), and `view add` takes a repeatable `--viewpoint` that it renders in flow style. `VIEWPOINTS` in the script and the table in `views.md` must change together.
The two existing views declare their viewpoints: `profile-lifecycle` as `[runtime, information]`, `run-selection-and-rendering` as `[runtime]`.
