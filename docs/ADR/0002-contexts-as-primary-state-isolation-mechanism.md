# ADR-0002 — Contexts as the Primary State-Isolation Mechanism

**Status:** Accepted  
**Date:** 2026-07-27

## Context

Halcyon must support a public shared world while preventing unrelated Players and Parties from advancing, blocking, or permanently altering one another's narrative state.

Implementing separate special-case systems for personal quests, Party quests, dungeons, public events, bounties, and narrative phasing would create duplicated logic and inconsistent behavior.

## Decision

Halcyon will use **Contexts** as the primary abstraction for scoping multiplayer state, participation, replication, persistence, and lifecycle.

Contexts may represent:

- Global state;
- Personal progression;
- Party progression;
- Narrative instances;
- Dungeon instances;
- Runtime Quests;
- Public Events;
- server-defined scenarios.

The exact Context data model remains subject to RFC and prototype validation.

## Consequences

Positive:

- one common abstraction can support several gameplay systems;
- replication and persistence can share scope rules;
- narrative isolation becomes explicit;
- Runtime Quests fit the same architecture;
- server operators and plugins gain a consistent model.

Negative:

- Context composition and conflict resolution are complex;
- the abstraction may become too broad;
- Player transitions require careful state reconciliation;
- Context-aware replication affects several existing systems.

## Constraints

- Contexts must not be presented as fully implemented.
- The first prototype must remain small.
- A Context must not automatically imply a fully separate cell instance.
- Players in different narrative Contexts should remain mutually visible when safe.
- Dedicated RFCs must define lifecycle, membership, composition, and persistence.

## Alternatives Considered

### Independent systems for each feature

Rejected because Party quests, personal quests, events, and instances would duplicate state-scope logic.

### Fully separate worlds per Player or Party

Rejected because it destroys the shared-world objective and increases storage and simulation cost.

### One global narrative state

Rejected because it allows one Player's decisions to break other Players' progression.

## References

- HTDS-006 — Architecture Principles
- HTDS-007 — Terminology
- HTDS-140 — World State
- HTDS-160 — Replication and Relevance
