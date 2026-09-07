---
status: accepted
date: 2026-09-07
retrospective: true
---

# 0001: Place application interfaces in contracts and implementations in use cases

## Context

The application layer separates Qt-free contracts from Qt-coupled orchestration. [`ksv_contracts`](../../../src/app/contracts/CMakeLists.txt) links only `ksv_domain` and enables `VERIFY_INTERFACE_HEADER_SETS`. Existing Qt-free interfaces include [`IGraphUseCase`](../../../src/app/contracts/i_graph_use_case.h), [`IScenarioBrowserUseCase`](../../../src/app/contracts/i_scenario_browser_use_case.h), and [`ISeriesManagementUseCase`](../../../src/app/contracts/i_series_management_use_case.h).

[`ISessionController`](../../../src/app/usecases/i_session_controller.h) is the sole current exception: it inherits `QObject`, uses `Q_OBJECT`, and exposes signals. `BenchmarkLibraryService` was once misplaced by treating this exception as the general pattern.

## Decision

Application-facing interfaces default to `src/app/contracts/`. An interface belongs in `src/app/usecases/` only when `QObject`, signals, or equivalent Qt coupling prevents it from compiling in Qt-free `ksv_contracts`.

Concrete implementations belong in `src/app/usecases/`, as illustrated by [`GraphUseCase`](../../../src/app/usecases/graph_use_case.h) and [`ScenarioBrowserUseCase`](../../../src/app/usecases/scenario_browser_use_case.h).

[`SessionController`](../../../src/app/session_controller.h) remains at the top of `src/app/` as a deliberate exception because it coordinates QObject workers and threads.

Rule of thumb: if an interface has no `QObject` or signals, put it in `contracts/` and its implementation in `usecases/`.

## Consequences

Qt-free application boundaries remain independently verifiable and depend only on the domain layer. Qt-coupled exceptions remain explicit.

`SessionController` is not a placement template for new Qt-free services. Mirroring it would repeat the mistake this decision exists to prevent.

## Alternatives considered

Placing new interfaces beside implementations by mirroring `SessionController` was rejected because it generalizes an exception and bypasses the Qt-free contracts boundary.
