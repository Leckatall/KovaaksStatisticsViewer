---
type: changed
area: Settings
user: Settings are now stored in an ini file (`%APPDATA%\Lecka\KovaaksStatsViewer.ini`) instead of the Windows registry. Settings saved by earlier versions in the registry are not carried over.
---
`SettingsService` now defaults to `QSettings::IniFormat` rather than `NativeFormat`.
`main()` calls `QSettings::setDefaultFormat(QSettings::IniFormat)` after the organization/application names are set, so the QML `Settings {}` blocks (window geometry, view toggles, history graph state) land in the same file as `SettingsService`.
There is deliberately no registry-to-ini migration in the app.
`ksv_gallery` redirects its `QSettings` ini path (and default format) into its throwaway temp dir: `SettingsService` hardcodes the production organization/application names, so the gallery previously wrote its temp Kovaak's dir and profile path into the real `KovaaksStatsViewer.ini`. Harmless while the app read the registry; destructive once the app reads the ini. `ksv_driver` already redirects into its session directory.
