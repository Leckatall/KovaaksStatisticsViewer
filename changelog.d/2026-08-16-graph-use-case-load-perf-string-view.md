---
type: fixed
area: Graphing
---
`GraphUseCase::load_perf()` now constructs `std::string(filename)` explicitly from the `string_view` parameter instead of passing `filename.data()` to the `setCurrentPerf(const std::string&)` overload, which read until the next NUL byte rather than respecting the view's own length — a risk for any non-NUL-terminated substring view. Also removed the `std::cout` diagnostic line in the same method.
