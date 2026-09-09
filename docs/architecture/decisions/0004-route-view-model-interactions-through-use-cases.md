---
status: accepted
date: 2026-09-09
retrospective: true
---

# 0004: Route view-model interactions through use cases

## Context

View models translate presentation state and user actions, while application use cases own workflows and coordinate services. Most current view models follow that boundary: `CompletionHistoryViewModel`, `GraphViewModel`, `PlaytimeGraphViewModel`, and `ScenarioBrowserViewModel` depend on use-case contracts, and `SessionViewModel` depends on the application controller contract.

Two view models currently bypass it. [`SettingsViewModel`](../../../src/ui/presentation/settings_vm.h) directly depends on `ISettingsService` and `IProfileService`, and [`BenchmarkManagerViewModel`](../../../src/ui/presentation/benchmark_manager_vm.h) directly depends on `IBenchmarkLibraryService`. The decision maker recalls these as expedient implementation shortcuts rather than intended exceptions.

Without an explicit dependency boundary, adding a direct service dependency can create a second path for application behavior and move workflow coordination into the presentation layer.

## Decision

View models depend on application use-case contracts for application behavior, state changes, queries, and event subscriptions. A controller contract may serve as that boundary when Qt signals or equivalent coordination require it. Use cases and controllers may depend on and coordinate services; view models do not depend directly on service contracts or concrete services.

This boundary is defined by responsibility rather than naming: a use-case contract expresses an application operation or workflow for a presentation client, while a service exposes a reusable capability or owns application or data state.

The existing direct service dependencies in `SettingsViewModel` and `BenchmarkManagerViewModel` are nonconforming architectural debt. They are neither sanctioned exceptions nor precedent for new view models.

This decision does not prevent view models from adapting presentation-only collaborators or using domain values in presentation state, and it does not require the existing violations to be migrated as part of recording this decision.

## Consequences

- Workflow and service coordination have a consistent owner in the application layer rather than being distributed across view models.
- View-model tests can substitute task-oriented use-case contracts instead of reproducing lower-level service interactions.
- Changes to service responsibilities can be absorbed behind use-case boundaries without necessarily changing presentation dependencies.
- Even a simple UI-to-service interaction may require a small use-case contract and implementation.
- The two existing violations remain migration debt until their application boundaries are introduced; leaving them temporarily in place does not weaken the rule for new work.

## Alternatives considered

Allowing view models to call services directly for simple features was rejected as the intended architecture. The current violations demonstrate the short-term convenience, but they also create competing interaction paths and assign application coordination to presentation code.

## Links

- [ADR-0001: Application interface and implementation placement](0001-application-interface-and-implementation-placement.md)
- [ADR-0003: Profile-independent benchmark repository and application-owned state](0003-profile-independent-benchmark-repository-and-application-owned-state.md)
