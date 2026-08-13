---
type: changed
area: Data & Profile
user: The default profile file is now profile.pb; existing installs using profile_cache.pb must select it again in Settings.
---
The protobuf schema, serializer, settings surfaces, tests and documentation now describe the
authoritative profile store instead of treating it as a derived cache.
