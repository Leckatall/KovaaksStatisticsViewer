---
type: fixed
area: Settings
user: The prompt to choose your Kovaaks folder no longer appears when the default Steam folder is already correct, and a banner now warns when the configured folder doesn't contain any Kovaaks data.
---
The folder prompts were keyed on `ISettingsService::isKovaaksDirSet()`, which only checked whether the `file/kovaaksDirs`/`file/kovaaks` keys existed. `getKovaaksDirs()` already fell back to the default Steam path, so a user whose default was correct still saw the welcome/banner and had Refresh Profile disabled until they re-picked the same folder; a user who picked a wrong folder got no feedback at all.
`isKovaaksDirSet()` is removed. `IFileService::hasValidSourceRoots()` (implemented in `FileService`) is true when there is at least one root and every root contains `FPSAimTrainer/performances` or `FPSAimTrainer/stats` (the two directories the importer reads), sharing the `kPerformancesSubdir`/`kStatsSubdir` constants with the scanner. Choosing the inner `FPSAimTrainer` folder of the Steam install therefore fails the check.
`SettingsViewModel` now takes `IFileService` and exposes `kovaaksDirValid` (replacing `kovaaksDirSet`, same `kovaaksDirChanged` notify), re-checked only when the folder is set — not on filesystem changes.
`FirstRunWelcome` shows when the folder is invalid and nothing is imported; `FirstRunBanner` is renamed `InvalidKovaaksDirBanner` and shows when the folder is invalid but runs exist. Refresh Profile is gated on validity.
