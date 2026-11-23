---
type: internal
area: Build & packaging
---
CI's vcpkg binary cache now actually gets reused. The old fixed key (`vcpkg-<commit>-<triplet>-<winlibs>`) was saved once from runner image 20261103; on image 20261116 every package's ABI hash differed (the mingw triplet's `VCPKG_ENV_PASSTHROUGH PATH`, plus the CMake/PowerShell versions vcpkg hashes), so each run restored the entry, reused 0 packages and rebuilt protobuf/abseil/gtest for 11–21 minutes, and the exact-key hit meant `actions/cache` never saved the fresh build.
The cache step is split into `actions/cache/restore` + `actions/cache/save`; the key ends in `github.run_id` and the restore takes the newest entry by prefix.
The install step prunes archives whose ABI is not in vcpkg's status database (`installed/vcpkg/status` + `updates/`), so dead packages are not carried forward, and reports `changed` when the kept set differs from what was restored.
The save runs only when `changed` and only on `refs/heads/main`, whose entries every branch and PR can read.
