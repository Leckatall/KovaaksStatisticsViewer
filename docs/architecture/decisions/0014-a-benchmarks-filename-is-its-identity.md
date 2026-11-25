---
status: accepted
date: 2026-11-25
retrospective: true
amends:
  - '0002'
---

# 0014: A benchmark's filename is its identity

## Context

[ADR 0002](0002-embedded-stable-identity-for-benchmark-elements.md) made an embedded, persisted UUID the identity of every benchmark as well as of every element within its definition, and treated the managed filename as an addressing concern only. Benchmark files were therefore named by a UUID unrelated to the benchmark's name.

The decision maker's account of the problem: the filename was opaque to users, and it was unintuitive that a benchmark's filename did not identify it. A benchmark file copied and pasted into the folder registered only once, because the copy carried the same embedded identifier. On examination, embedded identity at the benchmark level was over-design that only hurt the user experience.

Identity of the elements inside a definition is a separate concern. Thresholds and selections refer to tiers and scenario entries by their embedded identifiers, and nothing in this decision changes that.

This record is retrospective: the choice was already implemented when it was recorded.

## Decision

We chose to identify a benchmark by its filename within the managed benchmarks directory. The filename without its `.json` extension is both the benchmark's identity and its name, and the stored definition carries no benchmark identifier or name. Any key the application uses to address a benchmark in memory lasts only for the session and is never persisted. A file renamed or copied outside the application is therefore a different benchmark, and renaming a benchmark in the application renames its file.

This decision governs identity at the benchmark level only. Identity of the elements within a definition (tiers, groups, scenario entries) remains as ADR 0002 decides. It does not govern the rules for which names are usable as filenames, nor how older stored files are migrated, which [ADR 0011](0011-older-stored-formats-are-migrated-outside-current-format-code.md) and its amendments cover.

## Consequences

- The filename a user sees is the benchmark's name. A copied file is a separate benchmark, and a file renamed in a file manager appears under its new name.
- Benchmark names are constrained to what is valid and unique as a filename in the folder, ignoring letter case, so some names that were previously accepted are refused.
- There is no continuity across changes made outside the application: a file renamed outside it, or one that disappears and returns, is a new benchmark, and nothing can re-attach selection or session state to it.
- Renaming a benchmark is a file operation that can fail separately from saving its content, so a save can succeed while its rename fails.
- The stored format no longer carries a benchmark identifier or name, so builds that predate this change cannot read the new files, and existing files had to be migrated and renamed.
- Consumers still address a benchmark by an in-memory key during a session; that key is simply not stable across sessions.

## Alternatives considered

- **Keep the embedded benchmark identifier but name files after their benchmarks.** Considered and rejected: further examination found that the embedded benchmark identifiers provided no value in any circumstance.

## Links

- [Benchmark definitions](../views/benchmark-definitions.md) in the Architecture Description: identity, stored form and naming as built.
