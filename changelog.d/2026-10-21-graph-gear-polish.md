---
type: changed
area: Graphing
user: The settings gear on each graph is now clearly visible on the dark theme, highlights while its quick-settings popup is open, and the popup has a proper frame.
---
`GraphGear` sets `icon.color` (Lucide SVGs use `currentColor`, which Qt renders black, and Fusion does not tint by default): muted at rest, full `buttonText` when hovered, focused or open.
Custom rounded hover/open background replaces Fusion's square bevel; the tooltip is suppressed while the popup is open.
`settings.svg` stroke reduced to 1.5 at 16px for crisp 1px lines; new Lucide `x.svg` replaces the "×" text close button.
Popup gains a bordered, rounded `palette.window` background. The shared tint and background live in the new `ChromeIconButton`.
Closing the popup after keyboard use returns focus to the gear with `Qt.TabFocusReason`, so its focus ring stays visible; a mouse-only open and close leaves no ring.
