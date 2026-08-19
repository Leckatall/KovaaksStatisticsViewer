---
type: changed
area: Graphing
user: Series configuration now uses the app-wide settings service.
---
`SeriesConfigStore` delegates document persistence, quarantine, and legacy disabled-column migration to `ISettingsService`; its dedicated QSettings backend was removed.
