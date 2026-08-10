---
type: internal
area: Build & packaging
---
Changelog assembly split out of the release procedure into its own `changelog-assemble` skill, so the
assembled sections can be previewed between releases; `/release` Step 2 now delegates to it rather than
describing assembly, which is what keeps preview and release from drifting. The skill produces text
only — file insertion moved into Step 3, before the fragment `rm`, so a failure there leaves the inputs
intact. The format rules moved out of the skill and `AGENTS.md` into three tracked documents in
`docs/templates/` (fragment, CHANGELOG.md, RELEASE_NOTES.md): `.gitignore` hides `.claude/`, `CLAUDE.md`
and `AGENTS.md`, so none of these conventions previously reached anyone cloning the repo. Rules are
written as `> **RULE** —` blockquotes next to the shape they govern, a marker that cannot be mistaken
for entry content. They are a format reference, not a description of existing CHANGELOG.md sections —
history keeps whatever format it was written in. The release-notes shape then split by change type at
the top level — `### Features & changes` (added/changed/removed, area grouping demoted to `####`) and a
flat `### Bug fixes` ordered by the area enum — so a reader scanning for a fix does not have to read
every area. CHANGELOG.md is unaffected and keeps area-then-bucket.
