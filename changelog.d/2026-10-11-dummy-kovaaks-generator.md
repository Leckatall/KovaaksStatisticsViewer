---
type: internal
area: Build & packaging
---

Added `scripts/make_dummy_kovaaks.py`, which generates a fake KovaaK's directory (`FPSAimTrainer/performances/*Performance.perf` plus matching `stats/*Stats.csv`) for testing the app on a machine without KovaaK's installed. The `.perf` protobuf is hand-encoded from `perf.proto` (entries are per-interval deltas, as the decoder sums them), five scenarios with an improving score trend over ~30 days, deterministic per `--seed`. Default output is `.temp/dummy-kovaaks`.
