---
type: internal
area: Documentation
---
`style/diagrams.md` gains a "Title and key" section. Every diagram carries its title inside the Mermaid code block as frontmatter `title:`, so it renders as part of the image, with the same text in `accTitle`: a noun phrase naming the subject, relationship, and boundary that the heading and drawing leave unclear, unique among the document's diagrams and same-subject diagrams elsewhere. The criterion follows statistical table-titling practice rather than C4's type-and-scope rule, since the rendered diagram already shows its type.
The key now explains notation only, as one list item per visual format, starting with the format. Omissions move to the scope; takeaways, behaviour, and element responsibilities move to prose or the conclusion. The "participant key" and declaring edge meanings in an introduction or caption are gone.
A scope that shows a subset must state its selection rule instead of "selected".
The type, scope, and concern line still sits immediately before the diagram. Local review adds the two-author test for titles and the notation-only key check. All four worked examples gain titles and list-form keys; the build example's scope states its selection rule.
