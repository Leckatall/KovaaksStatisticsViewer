# View Style

A view answers one bounded architectural question. Read the [global principles](../README.md), [structural rules](../structure.md), and [diagram guidance](diagrams.md). Read [root style](root.md) before assessing or changing root summaries.

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

## Context, interfaces, and refinement

Give enough local context to understand the concern, then identify the authoritative parent or related account. Name the element or relationship being refined and map its external responsibilities to the detailed model. A refinement must not silently change a boundary, interface, or relationship's meaning.

Explain architecturally significant interfaces through purpose, exchanged information, ownership, and important interaction constraints. Include ordering, error propagation, lifetime, or compatibility when a maintainer needs them to preserve behavior. Do not turn this into an API reference or enumerate incidental signatures.

A compact responsibility table can clarify the participants. Runtime participants must map to the structural elements, or explicitly identify an instance, role, or worker within them. Distinguish object ownership from thread execution and storage ownership.

## Representative scenarios and quality

Select a few scenarios that expose the significant mechanism: a principal request, critical external interaction, startup/shutdown behavior, or failure/recovery path where relevant. Explain the trigger, starting state, important interaction, and resulting state. Exclude routine variants that establish no additional consequence.

Show asynchronous handoffs and acceptance boundaries explicitly. A request being queued does not mean it completed; a computation completing does not mean its result was committed. Show alternatives when they materially change the account.

Connect quality discussion to these mechanisms. Explain which response supports a quality goal and its limits; apply the [global distinction between goals, mechanisms, and measured outcomes](../README.md#evidence-and-accuracy). Do not invent a response-time target or imply a worker eliminates every source of blocking.

## Annotated example: background import

Everything in this example is fictional, including its evidence locators. The account illustrates writing, not a repository implementation.

### Concern and scope

How does Record Desk publish a worker's completed import without replacing accepted history with a failed batch? This view begins when the interface submits one batch and ends with either a committed history or a failure notification. It expands the import coordinator and worker introduced in the root. Export production and history browsing are outside this concern.

### Architecture

The interface submits a batch to the import coordinator. The coordinator owns the active request and queues decoding to the import worker. The worker produces either validated records or a validation error; it cannot write history.

On a successful worker result, the coordinator asks the history store to commit the batch atomically. The interface receives completion only after commit succeeds. A validation error or failed commit produces a failure notification and leaves the previously committed history intact. The [worker diagram](diagrams.md#worker-sequence-example) models these alternatives.

Illustrative evidence locators: `ImportCoordinator::acceptResult` establishes completion ordering; `HistoryStore::commitBatch` establishes atomic publication. These are fictional symbols, not links to existing code.

### Consequences and invariants

Worker completion and history publication are distinct events. Moving the completion notification ahead of the store's success response would allow the interface to report records that were never committed. Keeping publication authority in the coordinator also prevents the worker from mutating accepted history independently.

### Evidence and limitations

The illustrative verification scope covers coordinator result handling and the store's commit contract. It does not establish durability under power loss, cancellation behavior, or maximum import latency. Those properties need their own evidence before being claimed.

### Why this account works

It names the boundary and result states, explains a significant failure path, places evidence by the supported claims, and ends with specific limitations. It does not substitute a long call trace for the publication invariant. Its responsibility division is reused consistently in the sequence example.

## Local review

**Must:** Satisfy the managed-view contract, preserve the concern's boundary, map refinements and participants, and make significant interface/ownership behavior unambiguous. Include relevant failure behavior when omitting it would misrepresent the concern.

**Should:** Use a small number of representative scenarios, explain surprising interactions beside their model, and keep implementation detail proportional to its consequence. Preserve conforming local presentation choices and avoid repeating other views' detailed accounts.
