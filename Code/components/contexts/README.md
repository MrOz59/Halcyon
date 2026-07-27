# Context prototype

Server-side state model for the Halcyon Context system.

**Status: Prototype.** See `docs/RFC/0001-context-system.md` for the proposal and
for what this module does *not* yet do.

## Scope

This module owns Context identity, membership, and the one scoped component
RFC-0001 defines (`ScopedLifeState`). It answers a single question:

> given a Player, which scoped values may that Player observe?

It performs no replication, no persistence, no serialization, and no protocol
work, and it is not wired into `Code/server`. Those belong to later steps of
RFC-0001 and are deliberately absent so that the open question about Context ID
placement in the protocol stays open.

## Build

The module is header-only and has no build target of its own. `Code/tests` adds
`Code/components` to its include path and compiles `contexts.cpp` against it.

A `component()` target should be added once a translation unit in the server
actually links against this code — an empty static library would exist only to
look symmetrical with the other components.

## Design notes

- **Ids are server-issued and start at 1.** `kInvalidContextId` is 0, so a
  value-initialized `ContextId` is never a valid scope. HTDS-200 section 8
  forbids deriving Context identity from a Party leader, Session, quest FormID,
  or cell, all of which change while the Context stays valid.
- **`PlayerId` is not a Session id.** RFC-0001 requires membership to survive a
  reconnect.
- **Absent state is not "alive".** `GetLifeState` returns `std::nullopt` when a
  Context never recorded a value; callers must fall back to base game data. A
  Context that has genuinely observed an Actor alive stores `dead = false`, which
  is a different thing.
- **Stale revisions are rejected, not applied.** `SetLifeState` requires a
  strictly newer revision, so a replayed or reordered mutation cannot resurrect
  an Actor. Revisions are tracked per `(context, entity)` pair.
- **Ordered containers.** `std::map` and `std::set` keep iteration deterministic
  while the prototype is being validated. This is a diagnosability choice, not a
  performance one; HTDS-200 lists efficient Context indexing as an open question.
- **Not thread-safe.** The prototype assumes it is driven from the server tick,
  like the existing services in `Code/server`.
