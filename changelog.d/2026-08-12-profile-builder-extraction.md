---
type: internal
area: Architecture
---
`ProfileBuilder` (`src/data/profile_builder.h`) — new `ksv_data` class holding only an `IFileService`, whose `build()` returns a finished `domain::UserProfile` for the current Kovaaks directory. It is the scan+decode+aggregate half of `ProfileService::generateProfileFromDirectory()`, lifted out verbatim and freed of every `ProfileService` member, so it can later run off the UI thread.

`ProfileService::setProfile(UserProfile)` is the other half: swap the profile in and fire `notifyProfileChanged()`. Deliberately **not** on `IProfileService` — a pure virtual there would force edits to the three hand-written fakes, and nothing outside `ksv_data` needs it yet. `saveProfile()` stays at the caller rather than inside `setProfile`, because the cache-hit branch of `loadProfile()` also routes through `setProfile` and must not write the cache back on every startup; `ProfileServiceTest.LoadProfileFromCacheDoesNotSave` now pins that.

`generateProfileFromDirectory()` collapses to `setProfile(ProfileBuilder{m_file_service}.build()); saveProfile();`, dropping one redundant deep copy of the whole run vector. Behaviour is unchanged throughout — groundwork for asynchronous profile generation.
