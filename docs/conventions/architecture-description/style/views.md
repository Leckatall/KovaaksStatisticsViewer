# View Style

A view answers one bounded architectural question. Read the [global principles](../README.md), [structural rules](../structure.md), and [diagram guidance](diagrams.md). Read [root style](root.md) before assessing or changing root summaries; it also owns [when a concern needs a view](root.md#when-detail-belongs-in-a-view) at all.

## Managed-view contract

Each view must make the following recoverable, with headings and order chosen for its narrative:

- **Concern and reader outcome:** the question answered and why it matters.
- **Scope and exclusions:** the elements and relationships covered, start/end conditions when relevant, and adjacent behavior left elsewhere.
- **Architectural account:** significant responsibilities, ownership, interfaces, structure, runtime/data behavior, and boundary-crossing mechanisms.
- **Consequences and invariants:** what a maintainer must preserve and why the shape matters.
- **Evidence:** stable implementation anchors adjacent to the material claims or model they support.
- **Evidence and limitations:** a concluding section with the bounded verification scope, unresolved uncertainty, and incomplete or unavailable coverage.

The concluding `Evidence and limitations` section precedes a registry when present: the registry remains the final nonblank block under [structural rules](../structure.md#managed-references).

This is a content contract, not a mandatory heading template. A small view can combine narrative sections; a complex view can use multiple models to answer the same concern. Avoid creating one view per diagram type.

## Viewpoints

A viewpoint is a way of modelling a concern: the questions it answers, the elements it talks about, what its relationships mean, and the models that suit it. The concern decides whether a view exists; the viewpoint decides how it is modelled. Never choose a viewpoint first and then look for a concern to fill it.

| Viewpoint | Questions | Elements | Principal relationship | Lead models |
|---|---|---|---|---|
| `structural` | What are the parts of this element, what is each responsible for, and which depend on which? | Subsystems, components, interfaces | “depends on”, “is part of”, “implements” | [Building-block or dependency view](diagrams.md#selecting-an-abstraction), responsibility table |
| `runtime` | What happens when something occurs, in what order, on which thread, and what does failure look like? | Running instances, roles, threads, workers | “calls”, “signals”, “queues work for” | [Sequence or focused runtime flow, state view](diagrams.md#selecting-an-abstraction) |
| `information` | What data exists, what makes two items the same, which copy is authoritative, and how does its stored form change between versions? | Data entities, stores, schemas, identifiers | “is derived from”, “is the authoritative copy of”, “is persisted in” | [Data-flow view, lifecycle state view](diagrams.md#selecting-an-abstraction), identity table |

The managed frontmatter's `viewpoints` list records which viewpoints' questions the view claims to answer. It is a claim for review, not a label for readers: a reviewer checks that each listed viewpoint's questions are answered for the concern, and that the content does not substantially answer a viewpoint the list omits. Views do not otherwise announce their viewpoint; each diagram's [type line](diagrams.md#1-admission) already states its model kind.

A **combined view** lists more than one viewpoint. Combine viewpoints in one view when its concerns are tightly coupled, meaning a reader of one needs the other to act safely. Keep separate concerns in separate views even when they touch the same elements. Prose may move between viewpoints freely, since a cause and its consequence often belong to different ones; each diagram still has one model kind and one relationship meaning per edge style.

## Context, interfaces, and refinement

Give enough local context to understand the concern, then identify the authoritative parent or related account. Name the element or relationship being refined and map its external responsibilities to the detailed model. A refinement must not silently change a boundary, interface, or relationship's meaning.

Explain architecturally significant interfaces through purpose, exchanged information, ownership, and important interaction constraints. Include ordering, error propagation, lifetime, or compatibility when a maintainer needs them to preserve behavior. Do not turn this into an API reference or enumerate incidental signatures.

A compact responsibility table can clarify the participants. Runtime participants must map to the structural elements, or explicitly identify an instance, role, or worker within them. Distinguish object ownership from thread execution and storage ownership.

## Modelling by viewpoint

### Structural

Scope is the element being refined. Show its parts, each part's responsibility, the dependencies among them, and how the parent's external relationships map onto those parts. Decompose only the parts whose internals matter to the concern; a part with no consequence for maintainers stays a single box. Typical invariants are dependency rules and responsibility boundaries: which part may depend on which, and which part alone may perform an action.

### Runtime

Scope is a trigger and its outcome. Select a few scenarios that expose the significant mechanism: a principal request, critical external interaction, startup/shutdown behavior, or failure/recovery path where relevant. Explain the trigger, starting state, important interaction, and resulting state. Exclude routine variants that establish no additional consequence.

Show asynchronous handoffs and acceptance boundaries explicitly. A request being queued does not mean it completed; a computation completing does not mean its result was committed. Show alternatives when they materially change the account. Typical invariants are ordering and authority: what must happen before what, and who may publish a result.

### Information

Scope is a body of data across its life. Establish its identity (what makes two items the same), its authority (which copy wins, and what is only input or projection), its storage (where and in what form it persists), and its evolution (versioning, migration, and what happens to content that cannot be read). Show lifecycle states when they have architectural consequences. Data timescales are versions and sessions rather than calls, so do not import thread or call-ordering detail unless the concern is combined with `runtime`. Typical invariants are identity and authority rules: what equality ignores, and which store is the recovery source.

## Quality responses

Connect quality discussion to the mechanisms above. Explain which response supports a quality goal and its limits; apply the [global distinction between goals, mechanisms, and measured outcomes](../README.md#evidence-and-accuracy). Do not invent a response-time target or imply a worker eliminates every source of blocking.

## Parent views

A view may summarise and link narrower child views. A parent view must still carry an architectural account of its own, usually a structural refinement of the subsystem its children detail. Test it by deleting its links and child summaries: architecture must remain. A page that would be left empty is navigation, not a view.

Child summaries in a parent view follow the [root summary rules](root.md#when-detail-belongs-in-a-view): what the child establishes, why a maintainer would follow it, and at most one consequence. Organise parents and children by architectural element rather than by user-facing feature, so a mechanism that serves several features has one home; a feature that crosses subsystems is reached through links from each, not given a view of its own for that reason alone.

## Annotated example: background import

Everything in this example is fictional, including its evidence locators. The account illustrates writing, not a repository implementation. Its frontmatter would declare `viewpoints: [runtime]`.

### Concern and scope

How does Record Desk publish a worker's completed import without replacing accepted history with a failed batch? This view begins when the interface submits one batch and ends with either a committed history or a failure notification. It expands the import coordinator and worker introduced in the root. Export production and history browsing are outside this concern.

### Architecture

The interface submits a batch to the import coordinator. The coordinator owns the active request and queues decoding to the import worker. The worker produces either validated records or a validation error; it cannot write history.

On a successful worker result, the coordinator asks the history store to commit the batch atomically. The interface receives completion only after commit succeeds. A validation error or failed commit produces a failure notification and leaves the previously committed history intact. The [worker diagram](diagrams.md#worker-runtime-example) models these alternatives.

Illustrative evidence locators: `ImportCoordinator::acceptResult` establishes completion ordering; `HistoryStore::commitBatch` establishes atomic publication. These are fictional symbols, not links to existing code.

### Consequences and invariants

Worker completion and history publication are distinct events. Moving the completion notification ahead of the store's success response would allow the interface to report records that were never committed. Keeping publication authority in the coordinator also prevents the worker from mutating accepted history independently.

### Evidence and limitations

The illustrative verification scope covers coordinator result handling and the store's commit contract. It does not establish durability under power loss, cancellation behavior, or maximum import latency. Those properties need their own evidence before being claimed.

### Why this account works

It names the boundary and result states, explains a significant failure path, places evidence by the supported claims, and ends with specific limitations. It does not substitute a long call trace for the publication invariant. Its responsibility division is reused consistently in the sequence example.

## Annotated example: history identity and versions

Everything in this example is fictional. Its frontmatter would declare `viewpoints: [information]`.

### Concern and scope

What makes two Record Desk records the same, and what happens to stored history when its format changes? This view covers accepted history from the moment the coordinator commits it, through every later application version that opens it. How a batch reaches the commit is the background import view's concern and is not repeated here.

### Architecture

| Data | Identity | Authority |
|---|---|---|
| Exported record | Source system ID plus export timestamp | Replaceable input; never consulted after acceptance |
| Accepted record | The same pair; the display title is ignored | The history store is the only authoritative copy |
| History view row | Derived from an accepted record | Read-only projection, rebuilt on demand |

The history store writes a format version with every file. On open, a current version loads directly, the one supported older version is migrated in memory and rewritten, and any other version is moved aside unread and the store starts empty. Illustrative evidence locators: `RecordKey` establishes identity; `HistoryFile::open` establishes the version outcomes.

### Consequences and invariants

Because identity ignores the title, a re-export with a corrected title updates a record rather than duplicating it; adding the title to equality would split one record's history in two. Because exports are not authoritative, deleting them is safe, and a file moved aside is the only copy of its records: recovery must restore that file, not re-import.

### Evidence and limitations

The illustrative scope covers identity and open-time version handling. It does not establish concurrent access by two processes or migration across more than one version step.

### Why this account works

Its scope runs across versions rather than across one request. The table states identity and authority together, so the reader sees which copy wins and why duplicates cannot arise. Its invariants name the plausible change that would break them. It links to rather than repeats the runtime account of how a commit happens.

## Local review

**Must:** Satisfy the managed-view contract, preserve the concern's boundary, map refinements and participants, and make significant interface/ownership behavior unambiguous. Include relevant failure behavior when omitting it would misrepresent the concern. Answer the questions of every viewpoint the frontmatter lists, and list every viewpoint whose questions the view substantially answers. A parent view carries an architectural account beyond its child summaries.

**Should:** Use a small number of representative scenarios, explain surprising interactions beside their model, and keep implementation detail proportional to its consequence. Lead each viewpoint's part of the account with one of its lead models. Preserve conforming local presentation choices and avoid repeating other views' detailed accounts.
