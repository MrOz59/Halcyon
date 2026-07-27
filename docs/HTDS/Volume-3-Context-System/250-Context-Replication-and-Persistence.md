# HTDS-250 — Context Replication and Persistence

| Field | Value |
| --- | --- |
| Document ID | HTDS-250 |
| Title | Context Replication and Persistence |
| Version | 0.1.0 |
| Status | Draft Skeleton |
| Maturity | Conceptual / Pre-Implementation |

## 1. Purpose

This document connects Contexts to replication, snapshots, revisions, persistence, and recovery.

## 2. Replication Principle

Context state should be sent only to eligible recipients.

Eligibility may require:

- active membership;
- observer permission;
- compatible client capability;
- spatial relevance;
- successful Context activation.

## 3. Context Snapshot

A Context snapshot may include:

```cpp
struct ContextSnapshot
{
    ContextDescriptor descriptor;
    MembershipDescriptor membership;
    Revision revision;
    std::vector<EntitySnapshot> entities;
    std::vector<ScopedChangeForm> changes;
    std::vector<RuntimeQuestSnapshot> runtimeQuests;
};
```

This is illustrative.

## 4. Snapshot Triggers

Potential triggers:

- initial connection;
- Context join;
- reconnect;
- revision gap;
- administrator resync;
- Context activation after suspension.

## 5. Context Deltas

After snapshot acknowledgement, the server may send revisioned deltas.

A delta should identify:

- Context;
- base revision;
- new revision;
- changed state;
- ordering requirements.

## 6. Membership Before State

The client should normally receive membership or Context descriptor before scoped state that depends on it.

Potential sequence:

```text
Context descriptor
Membership
Snapshot
Acknowledgement
Subsequent deltas
```

## 7. Persistence Records

Potential records:

- Context descriptor;
- policy;
- lifecycle;
- membership;
- scoped Change Forms;
- Context revision;
- Runtime Quest state;
- archival metadata.

## 8. Durability

Different Context types may use different durability:

| Context | Typical durability |
| --- | --- |
| Global | Durable |
| Personal | Durable |
| Party | Durable or policy based |
| Narrative Instance | Durable while active |
| Dungeon | Durable or resettable |
| Runtime Quest | Durable while active |
| Public Event | Recoverable or Durable |

## 9. Recovery

After restart:

1. load Context descriptors;
2. restore lifecycle;
3. restore memberships;
4. restore scoped changes;
5. validate plugin manifest;
6. rebuild subscriptions after Session reconnect;
7. send snapshots.

## 10. Revision Model

Potential revision levels:

- Context revision;
- membership revision;
- scoped component revision;
- snapshot revision.

The smallest practical model should be chosen.

## 11. Deletion and Archival

Deletion should not silently orphan:

- scoped state;
- membership;
- rewards;
- audit records;
- dependent Contexts.

Archival may compact state while preserving history.

## 12. Persistence Failure

If critical Context state cannot be persisted:

- the mutation should be rejected;
- or the Context should enter degraded or suspended state.

The server should not claim durable completion.

## 13. Compatibility

Legacy clients without Context capability may:

- receive only Global state;
- be denied entry to Context-dependent servers;
- use a restricted compatibility mode.

The server must not leak private scoped state to legacy clients.

## 14. Initial Prototype

Persist:

- two Personal Contexts;
- memberships;
- one Actor Life State per Context;
- revisions.

On reconnect:

- each Player receives the correct snapshot;
- no cross-Context delta is sent.

## 15. Open Questions

- One snapshot per Context or composed Player snapshot?
- How are snapshot chunks ordered?
- Can Context state be lazy-loaded?
- Which Contexts stay in memory?
- How are archived Contexts compacted?
- Should Runtime Quest state share the same revision?
- How does mod-manifest change invalidate stored Context state?

## 16. Implementation Status

```text
Context snapshots: Not implemented
Context deltas: Not implemented
Context persistence: Not implemented
Context recovery: Not implemented
Legacy capability policy: Not implemented
```
