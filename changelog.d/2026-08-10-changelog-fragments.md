---
type: internal
area: Architecture
---
Changelog split into a technical `CHANGELOG.md` and a user-facing `RELEASE_NOTES.md`, both fed by
per-change fragments in `changelog.d/`. Fragments carry `type`/`area`, an optional `user` line that
promotes the change into the release notes, and a developer body; they are assembled and deleted at
version bump. Chosen over a git-hook design so entries survive `--amend`/rebase and are committed with
the change they describe. `.gitignore` needed `!changelog.d/` — the `*.d` prerequisite rule matched the
directory.
