---
type: fixed
area: Data & Profile
user: Fixed the app failing to find your KovaaKs install when no directory had ever been explicitly configured.
---
`SettingsService::getKovaaksDirs()` no longer pipes the hardcoded default directory literal through `QUrl(...).toLocalFile()` — that round-trip is only valid for values genuinely written via `QUrl::fromLocalFile(...)` (the legacy `file/kovaaks` key). `QUrl` was parsing the literal's `C:` as a URL scheme rather than a drive letter, so `toLocalFile()` silently returned an empty string whenever nothing had ever been configured.
