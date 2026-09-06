# Root Style

The root is an orientation map for a maintainer deciding where to look next. Read the [global principles](../README.md) and [structural rules](../structure.md) alongside this guide; use [diagram guidance](diagrams.md) for its models.

## Content and depth

The root must communicate the system's identity, purpose, audience, and as-built scope; external actors, systems, interfaces, dependencies, and owned data; significant drivers, constraints, and observed solution strategy; top-level responsibilities, boundaries, and relationships; genuinely shared facts; summaries of top-level concerns with managed links; and corpus-wide risks or evidence limitations.

Use whatever headings and order communicate this most clearly. Explain the system before presenting its navigation. A list of links alone cannot establish orientation, and a full copy of every view obscures it.

A context diagram and a top-level building-block diagram are normally useful. Omit or combine them only when they add no distinct architectural meaning. Context distinguishes the application boundary and its environment; building blocks explain responsibilities within that boundary. Avoid placing method-level interaction or an exhaustive evidence inventory in either overview.

## When detail belongs in a view

Extract a coherent account when it answers an independent reader question, needs a substantial local flow, has a distinct evidence boundary, or needs a separate maintenance context. Do not split merely because a section has become long or crosses a repository layer.

The root directly summarises top-level concerns. A narrower concern can be introduced by the relevant parent view, provided the reader can reach it through managed links and understands its relevance. A root summary should identify what the concern establishes and why a maintainer would follow it; it need not replay the child's steps.

Keep brief context where needed, following [ownership rules](../README.md#consistency-and-ownership). For example, a summary may mention a background worker to explain responsiveness, while a view owns worker completion, cancellation, and failure semantics.

## Annotated example: orientation and responsibilities

The following application and behavior are fictional.

> Record Desk is a local application for importing exported records and browsing an accepted history. Its users select input files and review import results. External exports are inputs; the application owns the history it has accepted. This account covers the desktop process and its local stores.
>
> Imports run in a worker so the interface can continue accepting interaction. A coordinator decides when a completed batch becomes visible; the worker does not publish history itself.

| Building block | Responsibility |
|---|---|
| Interface | Collect import requests and display the accepted history and import status. |
| Import coordinator | Own the active request and accept or reject its completed result. |
| Import worker | Decode and validate records outside the interface thread. |
| History store | Persist accepted records and expose the committed history. |

This explains responsibilities without cataloguing classes. It distinguishes who computes a result from who may publish it. An actual root would place implementation anchors beside these claims under the global evidence rules.

## Annotated example: concern summary

> Background import explains how a request crosses the worker boundary and becomes an accepted history update. Consult it when changing completion handling: a rejected batch must not replace the previously accepted history.

In an actual root, “Background import” would be a managed link with a local registry definition under the [reference rules](../structure.md#managed-references). The example is plain prose so it does not depend on an example file.

The summary provides a reason to navigate and a consequence to keep in mind. It deliberately leaves queueing, validation, and completion order to the view. A weak summary such as “See import details” provides no reader outcome; a paragraph repeating the full sequence creates another maintained account.

## Local review

**Must:** The root establishes orientation, communicates applicable content above, identifies top-level concerns through meaningful managed summaries, and keeps detailed concern ownership clear. Global facts must not become a dumping ground for locally significant detail.

**Should:** Prefer a short responsibility table to an inventory; keep context and internal decomposition distinct; make the next useful view easy to find. Preserve a readable narrative rather than requiring a fixed section template.
