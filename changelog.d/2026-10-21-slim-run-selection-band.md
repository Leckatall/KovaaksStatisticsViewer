---
type: changed
area: Graphing
user: Scenario history marks the selected run with a slim translucent cyan band instead of a bright line and rings around its values.
---
`GraphCanvas::drawMarkedX` paints an 8-pixel selection band at 10% opacity and a small axis tick, clipped to the plot. Selection is drawn behind axes, reference lines and series so individual values retain their normal appearance; enlarged selection rings are removed. The default marker colour follows the app's cyan selection styling. Rendering regressions cover the band, unchanged series points, edge clipping and missing or out-of-range selections.
