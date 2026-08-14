---
type: changed
area: Data & Profile
---
Profile stores now use a file wrapper whose leading header exposes the format version, creation time, and store name before the run body is parsed. `ProfileSerializer` rejects malformed headers and incompatible header versions before loading runs, while `readHeader()` provides side-effect-free metadata inspection.
