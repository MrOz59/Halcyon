# HTDS-150 — Entity Model

| Field | Value |
| --- | --- |
| Document ID | HTDS-150 |
| Title | Entity Model |
| Version | 0.1.0 |
| Status | Draft Skeleton |
| Maturity | Conceptual / Pre-Implementation |

## 1. Purpose

This document defines the initial vocabulary and structural hypothesis for server-recognized Entities.

An Entity is a multiplayer object with identity and state.

Not every Skyrim Form must become an Entity.

## 2. Current Reality

Tilted Evolution already has concepts for players, actors, objects, ownership, and server IDs.

Halcyon should preserve proven mappings where possible.

The target model adds:

- explicit Context binding;
- revisioned state;
- explicit authority;
- normalized game-data identity;
- persistent and ephemeral Entity classes;
- scoped variants.

## 3. Entity Categories

Potential categories include:

### Player Entity

Represents one active Character in the world.

### Actor Entity

Represents an NPC, creature, summon, or other Actor.

### Reference Entity

Represents a non-Actor placed or runtime object.

Examples:

- doors;
- containers;
- activators;
- furniture;
- quest objects.

### Projectile Entity

Represents a short-lived projectile when authoritative handling requires it.

### Runtime Entity

Represents a server-created object not originating directly from base plugin data.

### Logical Entity

Represents a server-side object without a direct Skyrim Reference.

Examples:

- a Runtime Quest;
- a bounty contract;
- a capture zone.

Whether Logical Entities belong in the same Entity system remains open.

## 4. Identity

Potential identifiers:

```cpp
using EntityId = StrongId<EntityTag, std::uint64_t>;
using ContextId = StrongId<ContextTag, std::uint64_t>;
```

Plugin-backed Entities may also have:

```cpp
struct NormalizedFormId
{
    PluginIdentity plugin;
    std::uint32_t localFormId;
};
```

Runtime Entity IDs must not collide with plugin-backed identities.

## 5. Entity Header

Conceptual header:

```cpp
struct EntityDescriptor
{
    EntityId id;
    EntityType type;
    ContextId primaryContext;
    AuthorityMode authority;
    std::optional<PlayerId> clientOwner;
    Revision revision;
    EntityLifetime lifetime;
};
```

This is illustrative.

## 6. Context Binding

Possible Context-binding models:

### Single Context

One Entity belongs to exactly one Context.

Simple, but may duplicate shared state.

### Multiple Context Membership

One Entity is visible in several Contexts.

Flexible, but complicates mutation ownership.

### Base Entity Plus Context Variants

One base identity has different scoped state per Context.

This model appears promising for narrative phasing.

Example:

```text
Base Entity: Paarthurnax Reference

Global Variant:
alive

Party 42 Variant:
dead

Party 57 Variant:
alive
```

The final model requires prototyping.

## 7. Components

Potential Entity components:

```cpp
struct TransformComponent;
struct ActorValuesComponent;
struct LifeComponent;
struct InventoryComponent;
struct EquipmentComponent;
struct AnimationComponent;
struct ReferenceComponent;
struct OwnershipComponent;
struct ContextComponent;
struct PersistenceComponent;
```

Components may support independent revisions.

The initial implementation should not introduce a full ECS unless it solves a concrete problem.

## 8. Authority

Every mutable component should have clear authority.

Examples:

- local movement may be client-owned but server-validated;
- Runtime Quest objectives are server-authoritative;
- reward state is server-authoritative;
- remote presentation is cosmetic;
- actor death may migrate from client-originated to server-authoritative.

Authority may vary by component rather than whole Entity.

## 9. Ownership

Ownership means responsibility for producing updates, not final truth.

A client-owned movement stream can still be rejected by the server.

Ownership transitions must be explicit.

Potential triggers:

- cell load;
- distance;
- Party leadership;
- NPC combat engagement;
- owner disconnect;
- server reassignment.

## 10. Lifetime

Potential lifetime classes:

- **Session** — exists only while one Session is active;
- **Encounter** — exists for one combat or event;
- **Context** — exists while one Context exists;
- **Persistent** — survives restart;
- **Base Reference** — derives from plugin data and persists through scoped changes.

## 11. Spawn and Despawn

Spawn should distinguish:

- creating an Entity record;
- enabling a Skyrim Reference;
- creating a local actor representation;
- subscribing one client to an existing Entity.

Despawn should distinguish:

- hiding from one recipient;
- removing from one Context;
- destroying ephemeral state;
- writing a durable tombstone.

## 12. Actor Model

Actor Entities may include:

- identity;
- base Form;
- current cell;
- transform;
- actor values;
- life state;
- inventory;
- equipment;
- animation state;
- combat target;
- ownership;
- Context variants.

Not every field needs server authority immediately.

## 13. Player Model

A Player Entity is associated with:

- persistent Player ID;
- Character ID, if adopted;
- active Session;
- Personal Context;
- optional Party;
- current transform;
- actor values;
- capabilities;
- permissions.

The Player Entity should survive Session replacement conceptually, even if the runtime representation is recreated.

## 14. Reference Model

Reference Entities may need:

- base Form;
- normalized Reference identity;
- transform;
- enabled state;
- lock state;
- inventory;
- activation state;
- Context-scoped overrides.

Doors and containers are likely useful early prototypes.

## 15. Entity Events

Examples:

```cpp
struct EntitySpawned;
struct EntityDespawned;
struct EntityComponentChanged;
struct EntityOwnerChanged;
struct EntityContextChanged;
struct EntityDestroyed;
```

Events should describe accepted state.

## 16. Entity Lookup

Potential indexes:

- by Entity ID;
- by normalized Form identity;
- by Context;
- by cell;
- by owner;
- by Entity type;
- by Player.

Indexes should be introduced based on actual queries and profiling.

## 17. Serialization

Entity state may be serialized differently for:

- network snapshots;
- network deltas;
- persistence;
- diagnostics;
- plugin APIs.

A single binary layout should not be assumed to serve every purpose.

## 18. Security

The server should reject:

- unknown Entity IDs;
- stale revisions;
- updates from non-owners;
- illegal Context transitions;
- impossible type changes;
- malformed component payloads.

## 19. Initial Prototype

The initial Entity prototype should model:

```text
One plugin-backed Actor Entity
One base identity
Two Context variants
One LifeComponent
One revision per variant
```

This is enough to validate the Paarthurnax-style isolation scenario.

## 20. Open Questions

- Should Logical Entities share the same store?
- Should Context variants be full Entities or component overlays?
- Which components need independent authority?
- How are runtime spawns mapped to Skyrim references?
- How are dynamic FormIDs normalized?
- How much of the existing server ID system can remain?
- Is an ECS useful, or would it add unnecessary complexity?

## 21. Implementation Status

```text
Existing actor and player representations: Implemented in Tilted Evolution
Unified Halcyon Entity model: Not implemented
Context variants: Not implemented
Normalized Form identity: Not implemented as specified
Component authority: Not implemented as specified
Persistent Entity lifetime: Partially exists, not implemented as specified
```
