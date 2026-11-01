---
type: internal
area: Architecture
---
`GraphScene` — a value type of owned, display-space graph state (resolved colours, series in line-list order, axes with every tick already formatted, carries kept apart from observations), holding no pointer, QObject or callable.
`GraphCanvas::scene()` resolves the canvas's effective state into it, and the free `paintGraphScene` paints it, so capture can replay the live canvas's path without a view model. The painter owns line stacking now, so `GraphCanvas::paintOrder()` is gone.
