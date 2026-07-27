# Context prototype

Server-side state model for the Halcyon Context system.

**Status: Prototype.** See `docs/RFC/0001-context-system.md` for the proposal and
for what this module does *not* yet do.

## Scope

This module owns Context identity, membership, and the one scoped component
RFC-0001 defines (`ScopedLifeState`). It answers a single question:

> given a Player, which scoped values may that Player observe?

`ContextStore.h` adds a text-file snapshot so that answer survives a restart.

It performs no replication and no protocol work. Wiring into the server lives in
`Code/server/Services/ContextService`, which is disabled by default; the open
question about Context ID placement in the protocol is deliberately left open.

`ContextStore` is **not** the persistence architecture of HTDS-170: no database,
no transactions, no schema migration, no audit trail, and no fsync. It is the
smallest thing that lets RFC-0001 steps 9-10 be exercised.

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
- **Loading is all-or-nothing.** A malformed store is refused outright instead
  of being applied up to the bad line, so a truncated file cannot silently drop
  half the scoped state. Saving writes to a temporary file and renames over the
  target, so an interrupted write leaves the previous snapshot intact. Neither
  is a durability guarantee: without fsync a host crash can still lose the most
  recent save.
- **Account identity is weak.** `ContextService` recognises a returning Player
  by their username, which the server neither verifies nor requires to be
  unique. Two Players claiming one name share a Personal Context. This is
  survivable for a local prototype and must be replaced before Contexts carry
  anything durable on a public server.
