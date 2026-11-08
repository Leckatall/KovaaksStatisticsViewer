---
type: internal
area: Architecture
---
`App`'s injected constructor now requires `data::IClipboardImageWriter` and `data::IPngImageWriter` after `daySource`, builds one `ImageOutputUseCase` over them and exposes it as `imageOutputUseCase()`. The delegating constructor supplies `QtClipboardImageWriter` and `QtPngImageWriter`; integration tests inject the fakes from `TestEnv`, and `ksv_gallery` and `ksv_driver` inject the inert writers in `tools/gallery/inert_image_output.h`, which write nothing and report failure. Nothing calls the use case yet.
