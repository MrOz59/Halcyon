# RFC-0001 — Context System

**Status:** Draft  
**Authors:** Halcyon Project Contributors

## Summary

This RFC proposes the first implementable version of the Halcyon Context system.

The initial version is deliberately narrow. It is intended to validate Context-aware replication before attempting complete quest synchronization or cell instancing.

## Problem

Two Players may occupy the same physical area while requiring different state for one quest-critical actor or object.

The server must prevent one Player's mutation from affecting the other while preserving shared Player visibility and optional PvP.

## Proposed Minimal Model

```cpp
using ContextId = std::uint64_t;

enum class ContextKind
{
    Global,
    Personal,
    Party,
    Instance
};

struct Context
{
    ContextId id;
    ContextKind kind;
    std::uint64_t revision;
};
```

This is a prototype interface, not a stable SDK.

## Membership

Each Player has:

- one Global Context;
- one Personal Context;
- zero or one active Party Context;
- zero or more temporary Instance Contexts.

The first prototype may support only Global and Personal Contexts.

## Scoped State

The first prototype stores one scoped component:

```cpp
struct ScopedLifeState
{
    ContextId context;
    EntityId entity;
    bool dead;
    std::uint64_t revision;
};
```

## Replication Rule

Player Entities remain globally visible.

The scoped actor Life State is sent only to Sessions whose active Context set contains the matching Context.

## Prototype Scenario

1. Player A and Player B enter the same cell.
2. Both Players see one another.
3. One quest-critical Actor exists for both.
4. A and B have separate Personal Contexts.
5. Player A kills the Actor.
6. Server stores `dead = true` in A's Personal Context.
7. B does not receive the death.
8. PvP between A and B remains functional.
9. Both disconnect.
10. After restart and reconnect, A sees the Actor dead and B sees it alive.

## Out of Scope

- quest stages;
- aliases;
- Party merging;
- nested Contexts;
- full cell instances;
- Runtime Quests;
- automatic branch detection;
- Context inheritance;
- mod compatibility.

## Persistence

The prototype should persist:

- Context descriptor;
- Player membership;
- Entity identity;
- Life State;
- revision.

## Protocol

The prototype requires:

- one Context capability;
- Context membership snapshot;
- scoped Life State message;
- revision;
- resync request.

## Risks

- Skyrim may locally recreate or resurrect the Actor;
- existing ownership paths may broadcast death globally;
- the same Reference may not tolerate divergent local state;
- reconnect application order may matter;
- AI and quest scripts may conflict.

## Acceptance Criteria

The RFC is validated when the prototype scenario succeeds reliably with two clients across server restart.

## Implementation Status

**Status: Prototype — state model only. The acceptance criteria above are NOT met.**

Implemented in `Code/components/contexts/`:

- `Context.h` — `ContextId`, `ContextKind`, `Context`, `ScopedLifeState`, and the
  `PlayerId` / `EntityId` identity types;
- `ContextRegistry.h` — Context creation with server-issued ids, full membership,
  the scoped Life State store, stale-revision rejection, and the replication rule
  from this document expressed as `IsVisibleTo` / `GetVisibleLifeStates`.

Covered by unit tests in `Code/tests/contexts.cpp`, including the divergence
described in steps 1-8 of the prototype scenario.

Not implemented:

- **Persistence.** Nothing in the section above is stored; the registry is
  in-memory only, so steps 9-10 of the scenario are unverified.
- **Protocol.** No capability, membership snapshot, scoped Life State message,
  or resync request exists. The open question about Context ID placement remains
  unanswered and is deliberately not pre-empted by the current types.
- **Client and world integration.** The registry is not wired into `Code/server`;
  no gameplay path reads or writes it, and no Actor death reaches it.
- **Every risk listed below.** All concern live Skyrim behavior and remain
  entirely untested.

The registry is a standalone module: validating its state transitions is a
precondition for the RFC, not evidence that the RFC succeeds.

## Open Questions

- Should the first Context ID be attached to Entity state or message envelope?
- Can existing server IDs represent Context variants?
- How is local actor state reapplied without breaking scripts?
- Should Context membership be cached client-side before game load?
