# ADR-0003 — Progressive Server Authority

**Status:** Accepted  
**Date:** 2026-07-27

## Context

The target Halcyon architecture is server-authoritative, but the inherited Tilted Evolution implementation contains working client-owned and client-originated systems.

A complete rewrite would risk losing functional gameplay, Linux compatibility, and protocol behavior before equivalent replacements exist.

## Decision

Halcyon will migrate authority progressively, subsystem by subsystem.

Each subsystem should declare its current and target authority mode.

Conceptual modes include:

- Legacy Client;
- Client-Owned, Server-Recorded;
- Server-Validated;
- Server-Authoritative;
- Cosmetic.

## Consequences

Positive:

- working behavior remains available during migration;
- prototypes can validate assumptions;
- regressions are easier to isolate;
- contributors can migrate one domain at a time.

Negative:

- mixed authority models will coexist temporarily;
- code may require compatibility adapters;
- documentation must remain explicit about current behavior;
- some exploits and divergence remain until migration completes.

## Rules

- New persistent reward systems should be server-authoritative from the beginning.
- Temporary client authority must be documented.
- Legacy behavior must not be described as the target design.
- Authority migration should include diagnostics and tests.
- Architectural purity alone is not sufficient reason to remove working behavior.

## Alternatives Considered

### Immediate full server-authoritative rewrite

Rejected due to excessive risk and lack of validated replacements.

### Preserve current client authority indefinitely

Rejected because it cannot safely support persistent public servers and server-owned gameplay.

## References

- HTDS-003 — Goals
- HTDS-006 — Architecture Principles
- HTDS-110 — Server Architecture
