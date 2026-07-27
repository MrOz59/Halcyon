# HTDS-170 — Persistence

| Field | Value |
| --- | --- |
| Document ID | HTDS-170 |
| Title | Persistence |
| Version | 0.1.0 |
| Status | Draft Skeleton |
| Maturity | Conceptual / Pre-Implementation |

## 1. Purpose

This document defines the initial persistence goals and data model hypothesis for Halcyon.

Persistent public servers require server-owned durable state that is not replaced by whichever local save reconnects first.

## 2. Current Reality

Tilted Evolution's current persistence behavior must be documented through source analysis.

Halcyon's target persistence model should support:

- Player identity;
- Character state;
- Parties;
- Contexts;
- scoped Change Forms;
- Runtime Quests;
- public events;
- bounties;
- rewards;
- audit records;
- schema migration.

## 3. Persistence Principles

Persistence SHOULD be:

- server-owned;
- revisioned;
- transactional where required;
- recoverable;
- observable;
- schema-versioned;
- isolated by domain;
- independent from one local save after import.

## 4. Base Data Plus Deltas

Halcyon should avoid storing full copies of Skyrim's base world.

Preferred model:

```text
Configured plugin data
+ durable global mutations
+ durable Context mutations
= recovered effective world
```

This reduces storage duplication and aligns with Context isolation.

## 5. Candidate Domains

Potential durable domains:

```text
players
characters
sessions_history
parties
contexts
context_memberships
entities
entity_components
scoped_change_forms
runtime_quests
runtime_objectives
public_events
bounties
rewards
plugin_manifests
audit_log
schema_migrations
```

These are conceptual, not a final SQL schema.

## 6. Player and Character Data

Potential Player data:

- Player ID;
- account identity;
- permissions;
- moderation state;
- created time;
- last seen.

Potential Character data:

- Character ID;
- Player ID;
- Personal Context ID;
- imported metadata;
- progression metadata;
- selected appearance;
- last known location.

The first implementation may combine Player and Character.

## 7. Context Persistence

A Context record may include:

```cpp
struct PersistentContext
{
    ContextId id;
    ContextType type;
    ContextLifecycleState state;
    Revision revision;
    TimePoint createdAt;
    std::optional<TimePoint> expiresAt;
    SerializedPolicy policy;
};
```

Context membership should be persisted when reconnect restoration requires it.

## 8. Scoped Change Forms

Potential key:

```text
Context ID
+ normalized Reference identity
+ component type
```

Potential payload:

- revision;
- changed-field mask;
- serialized state;
- last mutation source;
- last updated time.

The format should support migration.

## 9. Runtime Quest Persistence

Runtime Quests may require:

- template identity;
- Context;
- lifecycle state;
- objectives;
- participants;
- contribution;
- expiry;
- rewards;
- completion state.

Critical reward state must not be granted twice after restart.

## 10. Transactions

Transactions should be used when partial completion would corrupt state.

Examples:

- consume bounty reward pool and grant reward;
- move Player between Parties and Contexts;
- complete Runtime Quest and record rewards;
- transfer persistent ownership;
- apply critical inventory grant.

Not every movement or actor-value update requires a transaction.

## 11. Idempotency

Requests that may be retried should carry stable operation identifiers.

Example:

```cpp
struct DurableOperationId
{
    SessionId originSession;
    CorrelationId correlation;
};
```

The server should recognize already-applied operations.

## 12. Write Ordering

Potential policy:

- critical durable state commits before success acknowledgement;
- normal durable state may use grouped commits;
- recoverable state may use periodic checkpoints;
- ephemeral state is not stored.

The trade-off between latency and durability must be explicit.

## 13. Database Backend

Potential initial choices:

### SQLite

Advantages:

- simple deployment;
- good for development;
- easy backups;
- no external service.

Limitations:

- write concurrency;
- operational scaling.

### PostgreSQL

Advantages:

- robust transactions;
- operational tooling;
- concurrency;
- remote access.

Limitations:

- more deployment complexity.

The architecture should use a persistence abstraction without hiding important backend semantics.

## 14. Schema Migrations

Every durable schema should have a version.

Migrations should be:

- ordered;
- repeatable or safely detected;
- tested;
- reversible when practical;
- backed up before destructive changes.

The server should refuse to start if an incompatible schema cannot be migrated safely.

## 15. Recovery

Recovery should:

1. validate schema;
2. validate plugin manifest;
3. load durable Player and Context records;
4. restore scoped Change Forms;
5. restore Runtime Quests and events;
6. repair or quarantine incomplete operations;
7. publish server readiness.

## 16. Local Save Import

A local save may be used as:

- initial Character import;
- migration source;
- diagnostic comparison.

It should not remain a continuing source of authority after the server accepts ownership.

Import should record provenance and version.

## 17. Backups

Server operators should be able to back up:

- database;
- plugin manifest;
- server configuration;
- game-mode configuration;
- migration state.

Backups should be possible without silent corruption.

## 18. Audit

Sensitive durable actions should produce audit records.

Examples:

- administrator grants;
- bounty payouts;
- moderation actions;
- inventory grants;
- context deletion;
- migration;
- rollback.

Audit records should identify actor, target, time, reason, and correlation ID.

## 19. Data Retention

Retention policy may differ by domain.

Examples:

- authoritative state: retained;
- audit: retained by policy;
- connection logs: rotated;
- movement history: not persisted;
- expired events: archived or summarized.

Server operators may configure retention within safe limits.

## 20. Failure Handling

Potential failure outcomes:

- retry;
- reject operation;
- enter read-only mode;
- quarantine one Context;
- stop server startup;
- restore checkpoint.

The server should not claim success after a critical persistence failure.

## 21. Initial Persistence Prototype

The first prototype should persist:

- one Context;
- one membership;
- one scoped actor life-state delta;
- one revision.

After restart:

- the Context is restored;
- the correct Player receives the actor state;
- an unrelated Player receives the base state.

## 22. Open Questions

- Event sourcing or current-state tables?
- One Entity-component table or domain tables?
- How should plugin-manifest changes invalidate state?
- Should Context deletion cascade or tombstone?
- How are large inventories serialized?
- Which operations require synchronous durability?
- How should backups coordinate with active writes?
- What is the first supported production backend?

## 23. Implementation Status

```text
Target persistence service: Not implemented
Context persistence: Not implemented
Scoped Change Forms: Not implemented
Runtime Quest persistence: Not implemented
Schema migration framework: Not implemented as specified
Local save import: Not implemented as specified
```
