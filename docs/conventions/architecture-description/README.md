# Architecture Description Conventions

These conventions govern the as-built Architecture Description: the root at `docs/architecture/README.md` and managed views beneath `docs/architecture/views/`. Those paths define the corpus; no current document is a required example or source of these conventions.

The corpus helps maintainers locate a concern, understand the significant structure or behavior, and verify it against implementation. The root provides orientation and shared facts; views explain bounded concerns.

## Reading and authority

| Guide | Owns |
|---|---|
| [Structural rules](structure.md) | Identities, drafts, reference syntax, registries, root facts, reachability, integration, and deterministic validation. |
| [Root style](style/root.md) | Orientation, summaries, depth, and placement of detail. |
| [View style](style/views.md) | Bounded accounts, interfaces, runtime scenarios, consequences, and local evidence presentation. |
| [Diagram style](style/diagrams.md) | Model selection, abstraction, notation, layout, and explanatory context. |

Read this page and structural rules for every AD operation. Diagram guidance applies to both root and views. Document operations also read both document styles; view operations read view style and additionally root style before assessing or changing root content. Operation-specific scope and write authority remain in the architecture-description skill.

This page owns shared semantic principles. Structural rules own mechanical interpretation; style guides specialise presentation without overriding either. Link to a shared rule rather than copying it. The ADR convention and recording workflow remain separate.

## Architectural significance

Include a detail when it materially explains structure, responsibility, dependency, interface, runtime or data flow, ownership, lifecycle, constraint, quality response, risk, stable identity, or decision status.

This test applies equally to prose, tables, and diagrams. Classes, functions, signals, settings, and files belong when they reveal a significant mechanism or invariant, not merely because they exist. Apply the test most strictly in the root. Avoid class catalogues, directory tours, exhaustive call traces, and inventories without architectural consequences.

Choose views by reader questions. A coherent concern may cross layers and directories; neither the file tree nor a diagram type requires a separate view.

## Evidence and accuracy

Every material claim or model must have sufficient current implementation evidence for the selected scope. A diagram has the same evidence burden as prose.

- Place stable repository paths or qualified symbols beside each material claim or tightly related cluster. Prefer them to fragile line-number citations.
- A diagram and adjacent explanation may share evidence when its supported scope is clear.
- State the verification boundary and disclose incomplete, unavailable, or uncertain evidence. Follow repository graph and source-coverage rules.
- Evidence must support verification of the concern without rediscovering the entire system. Existing citations do not remove the obligation to check current implementation during updates and audits.

Distinguish implemented fact, accepted intent, recommendation, developer recollection, bounded inference, credible rejected alternative, consequence, implementation latitude, and unresolved question. Accepted ADRs establish intent; they establish current architecture only when implementation evidence agrees. Never infer design rationale from an elegant implementation or invent alternatives and chronology. Resolve material ambiguity before writing the affected claim.

Distinguish a quality goal, an implemented response, and a measured outcome. A worker mechanism may support responsiveness without proving a latency target. Include metrics only when evidence or an explicitly accepted requirement establishes them, and label which kind they are.

## Consistency and ownership

An element retains its canonical name, type, responsibility boundary, and relationship meaning wherever it appears. Map a local abbreviation to its canonical name at first use. A refinement identifies the parent element or relationship it expands.

Each detailed account has one authoritative home:

- The root owns orientation and genuinely shared facts.
- A managed view owns its concern's detailed account.
- A parent summary explains a child's relevance and provides a managed link.
- Reusable shared claims use [root facts](structure.md#root-facts) when canonicalisation is warranted.

Brief context may recur so a view is readable independently; it must identify the authoritative account and must not become a second maintained explanation. Do not copy whole models or prose for convenience. Resolve inconsistent implementation evidence or explicitly record the inconsistency; locally coherent but incompatible views cannot silently coexist.

## Corpus completeness

Judge completeness across the root and reachable views. Consider the following concerns when relevant to the implemented system and selected operation:

- Purpose, stakeholders, reader questions, and system boundary.
- External systems, dependencies, interfaces, and owned data.
- Drivers, constraints, principles, and solution strategy.
- Static structures, responsibilities, ownership, and dependencies.
- Significant runtime interactions and data flows.
- Data identity, persistence, migration, and compatibility.
- Build, deployment, and operational topology.
- Cross-cutting mechanisms and quality responses.
- Risks, debt, uncertainty, and evidence limitations.
- Relevant accepted decisions or governing designs.
- Terminology that affects architectural interpretation.

This is a relevance checklist, not a required table of contents. Do not add empty or “Not applicable” sections. During audits, report a missing applicable concern as a semantic gap and explain the reader task it blocks. Apply this within the operation's selected scope; the checklist does not authorise a whole-corpus audit.

## Authoring and review

A **must** failure blocks the affected proposed addition or update. A **should** finding identifies a concrete readability improvement; it does not have the same authority as a correctness defect. Audit operations report findings without changing files.

Global must criteria are a bounded concern, accurate evidence-backed claims and models, appropriate root/view ownership, consistent names and mappings, disclosure of uncertainty, and coverage of applicable concerns or explicit gaps. Apply the additional local criteria in each style guide.

Authors should prefer concise narrative, small responsibility tables, stable evidence anchors, and conclusions adjacent to the model that establishes them. Preserve conforming local presentation choices. Headings and section order are generally flexible; explicitly stated mechanical or content constraints still apply.

Retain the operation guides' architecture-conformance categories. A style failure outside those categories is an **authoring-quality gap**, distinct from structural failures and integration warnings. Never use that label to soften an incorrect or stale architectural claim.

## Sources and adaptation

These are influences on local rules, not a claim of formal conformance. The IEEE source consulted is the public 2022 overview, not the full standard.

| Source | Adopted guidance and local adaptation |
|---|---|
| [ISO/IEC/IEEE 42010-2022 overview](https://standards.ieee.org/ieee/42010/6846/) | Distinguish architecture from its description; the standard overview does not prescribe a recording format. Our Markdown organisation and managed identities are local choices. |
| [SEI Views and Beyond](https://www.sei.cmu.edu/library/views-and-beyond-collection/) | Select useful views for readers and maintain information that applies across views. Our root and managed-view model tailors this principle. |
| C4 [notation](https://c4model.com/diagrams/notation), [system context](https://c4model.com/diagrams/system-context), [review checklist](https://c4model.com/diagrams/checklist), and [scaling guidance](https://c4model.com/faq#does-the-c4-model-scale) | Make abstractions, responsibilities, and relationships understandable; keep context black-box and split large diagrams around a specific focus. C4 generally favours explicit relationship descriptions. Locally, relationships that differ remain explicit, while repeated visually identical edges may declare their shared meaning once to avoid redundant visual noise. This is an intentional adaptation, not a claim of C4 conformance. |
| arc42 [building blocks](https://docs.arc42.org/section-5/), [runtime scenarios](https://docs.arc42.org/section-6/), and [quality](https://docs.arc42.org/section-10/) | Use selective refinement, useful responsibility descriptions, representative scenarios, and concrete quality responses. We do not mandate arc42's headings or present unmeasured quality goals as implemented guarantees. |
| [Google technical-writing illustration guidance](https://developers.google.com/tech-writing/two/illustrations) | Write the takeaway before drawing, keep one diagram explainable in roughly one paragraph or no more than about five short bullets, reveal complexity progressively, and iterate after rendering. The paragraph-or-five-bullets measure is a review warning, not a universal limit. |
| [Microsoft architecture-diagram guidance](https://learn.microsoft.com/en-us/azure/well-architected/architect-role/design-diagrams) | Maintain a minimal purposeful set, layer views rather than overloading one, label relationships when their meaning is not immediately evident, keep notation consistent, and account for accessibility. Our three quality gates make these reviewable locally. |
| Daniel Moody, [“The Physics of Notations”](https://ieeexplore.ieee.org/document/5353439) | Use cognitive fit, complexity management, graphic economy, and perceptual discriminability to review whether a visual notation suits its reader and task. These principles inform our rubric; we do not claim formal conformance to the paper's framework. |
| Purchase et al., [UML layout-preference study](https://courses.ischool.berkeley.edu/i247/f05/readings/Purchase_UML_OzIV01.pdf) | Prefer semantic grouping, narrative-compatible layout, and fewer avoidable edge crossings. The study informs qualitative review and does not establish a universal node or edge threshold. |
| Mermaid [accessibility support](https://mermaid.js.org/config/accessibility) and Google [accessibility guidance](https://developers.google.com/style/accessibility) | Provide `accTitle` and `accDescr`, preserve the essential information in adjacent prose, and never make colour the sole carrier of meaning. |

## Examples

The style guides use a fictional local record-import application. Their prose, model behavior, and evidence locators illustrate authoring choices; none establishes facts about this repository. All example content needed to understand the guidance is embedded in this collection.
