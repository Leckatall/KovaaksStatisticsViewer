---
type: fixed
area: Settings
user: The expression editor dialog now resizes itself to fit the expression you're editing, and scrolls instead of clipping when it's too big for the window.
---
`ExpressionTreeEditor` now reports its content-driven implicit size, keeps nested expression cards at their natural height, and scrolls deep chains within the available space. `ExpressionEditorDialog` clamps its size to the popup overlay.
