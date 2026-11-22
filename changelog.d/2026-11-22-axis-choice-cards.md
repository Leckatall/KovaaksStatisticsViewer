---
type: changed
area: Settings
user: Axis choices in graph settings are now selectable cards with a radio marker — click anywhere on a card to pick that axis — and deleting a shared axis uses a trash button.
---
`graph/AxisChoiceCard.qml` replaces `graph/ShowOnAxisButton.qml` for the rank graphs' horizontal axis (`GraphSettingsEditor.qml`) and the run graph's Y-axis label choice (`AxesPanel.qml`). It is an `ActionButton` whose `checked` follows `selected` (the VM's `shown`) and which emits `chosen(key)` only when unselected; a `header` slot holds the shared axis's name field and trash button, which keep their own clicks and Tab stops (card → name → Delete), and the default slot holds the detail lines. Keyboard focus draws a ring in `palette.highlight`, apart from the `palette.link` selection outline; every colour comes from existing `Theme` roles.
`ActionButton`'s Enter handling now acts only while the button itself has active focus: a `TextInput` leaves Return unaccepted after emitting `editingFinished`, so Return in a card's name field reached the card and would have chosen that axis.
The narrow-width single-column layouts in `AxesPanel` and the X-axis rows are gone; the card keeps one layout and wraps its details.
Object names: `showOnXAxis_<key>` / `graphXAxis_<key>` become `xAxisChoice_<key>`; `showOnYAxis_<key>`, `axisRow_<id>` and `independentRow_<id>` become `yAxisChoice_<key>`; `independentName_<id>` is gone (the card's own name label). Delete is an icon-only `trash-2.svg` button with a "Delete axis" tooltip, keeping `deleteAxis_<id>` and its accessible name.
