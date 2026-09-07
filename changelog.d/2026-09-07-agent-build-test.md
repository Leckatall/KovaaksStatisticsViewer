---
type: added
area: Build & packaging
---
Added `scripts/build_and_test.py` as the deterministic Debug build and verification entry point, with logical test scopes, wildcard matching, compact success output, long-running progress estimates, direct failure diagnostics, and retained full logs that do not require a separate read step.
Build concurrency is fixed internally at eight compiler jobs rather than exposed as an agent-controlled option.
Toolchain paths now resolve through tracked defaults, an ignored local override, and process environment variables; the Release packager shares the same loader while keeping explicit parameters at highest precedence.
The agent build validates and fingerprints its incremental CMake tree, and Protocol Buffer generation stages `protoc` with its required runtime DLLs inside the build instead of modifying the vcpkg installation.
