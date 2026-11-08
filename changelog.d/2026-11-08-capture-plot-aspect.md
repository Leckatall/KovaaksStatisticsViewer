---
type: changed
area: Graphing
---
`graphReferenceSize` now sizes a graph capture by the shape of its plot, not of the image: `CaptureComposition` replaces `standardGraph` (960×600) with `standardWidth` (960) and `plotAspect` (3.0, plot width over plot height), and the reference height is the smallest whole-pixel height at which the layout fits and the plot is no wider than that ratio.
Before, the plot got whatever the fixed 960×600 page left after headings, axis titles and legend, so its shape varied by graph (about 2.0:1 for a one-row rank legend, 2.45:1 for a four-row run legend) and every extra legend or title line made it flatter. Now that chrome adds height instead.
`graphCapturePlot(const CaptureLayout &)` exposes the plot rect exactly as the renderer paints it; the search and the tests measure through it.
3.0 was chosen by comparing real-font samples at 3.0, 3.5 and 4.0 against the old layout; the samples test records each graph sample's plot ratio as a test property.
