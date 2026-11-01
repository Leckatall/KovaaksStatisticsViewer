---
type: changed
area: Benchmarks
---
`BenchmarkGroupCommand`'s panel is translucent. The alpha is on the background's colour rather than the `Popup`, so the text and input controls stay opaque.
Its actions are now cards in the style of `ScenarioRunItem`: the highlighted one takes the accent tint and border, and each card draws its own `Label` because Fusion's `ItemDelegate` paints highlighted text in `highlightedText`, which is unreadable on that tint.
