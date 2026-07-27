# HTDS-210 — Context Types and Policies

| Field | Value |
| --- | --- |
| Document ID | HTDS-210 |
| Title | Context Types and Policies |
| Version | 0.1.0 |
| Status | Draft Skeleton |
| Maturity | Conceptual / Pre-Implementation |

## 1. Purpose

This document proposes initial Context categories and policy fields.

Types should describe common intent without creating hard-coded behavior for every gameplay feature.

## 2. Proposed Context Types

```cpp
enum class ContextType
{
    Global,
    Personal,
    Party,
    Narrative,
    Dungeon,
    RuntimeQuest,
    PublicEvent,
    ServerDefined
};
```

This list is provisional.

## 3. Global Context

Purpose:

- state shared by the broad server population.

Potential state:

- public Players;
- public chat;
- global server rules;
- world events;
- global weather;
- public PvP policy.

There may be one default Global Context or several region-specific Global Contexts later.

## 4. Personal Context

Purpose:

- persistent state owned by one Character.

Potential state:

- solo quest progress;
- personal branch decisions;
- personal quest Actors;
- private scripted References.

A Personal Context should survive disconnect.

## 5. Party Context

Purpose:

- shared cooperative progression.

Potential state:

- active quest progress;
- Party dungeon state;
- puzzle state;
- shared objective state;
- quest-critical Actor state.

Joining a Party must not automatically destroy Personal state.

## 6. Narrative Context

Purpose:

- isolate one incompatible story state.

Examples:

- Paarthurnax killed or alive;
- faction branch;
- city destruction state;
- scripted scene state.

A Narrative Context may be attached to a Personal or Party Context rather than always standing alone.

## 7. Dungeon Context

Purpose:

- isolate a dungeon or bounded set of cells.

Potential state:

- enemy spawns;
- doors;
- puzzles;
- containers;
- boss state;
- reset rules.

## 8. Runtime Quest Context

Purpose:

- own one server-created activity.

Potential state:

- participants;
- objectives;
- spawned Entities;
- timers;
- contribution;
- rewards.

## 9. Public Event Context

Purpose:

- allow many unrelated Players to participate in shared server content.

Unlike a Party Context, participation may be temporary and non-exclusive.

## 10. Server-Defined Context

Purpose:

- allow approved plugins or administrators to create custom scopes.

Server-defined Contexts should declare explicit policy rather than relying on unknown defaults.

## 11. Policy Model

Conceptual policy:

```cpp
struct ContextPolicy
{
    MembershipPolicy membership;
    VisibilityPolicy visibility;
    MutationPolicy mutation;
    PersistencePolicy persistence;
    ConflictPolicy conflict;
    LifecyclePolicy lifecycle;
};
```

## 12. Membership Policy

Potential modes:

- invite only;
- owner managed;
- Party managed;
- automatic by region;
- automatic by quest;
- public join;
- server assigned;
- plugin controlled.

## 13. Visibility Policy

Potential modes:

- members only;
- members and guests;
- public Players, private Entities;
- public event participants;
- administrator visible;
- fully isolated.

## 14. Mutation Policy

Potential controls:

- server only;
- Context owner;
- Party leader;
- validated member Intent;
- approved plugin;
- administrator.

## 15. Persistence Policy

Potential modes:

- ephemeral;
- session durable;
- restart durable;
- permanent;
- archive on completion;
- delete after expiry.

## 16. Conflict Policy

Potential modes:

- reject activation;
- highest precedence;
- isolate Entity;
- isolate component;
- create child Context;
- require administrator resolution.

Automatic merging should not be the default for narrative state.

## 17. Lifecycle Policy

Potential controls:

- owner lifetime;
- Party lifetime;
- fixed expiry;
- completion driven;
- manual;
- persistent until archived;
- reset interval.

## 18. Context Capabilities

Contexts may declare capabilities:

```text
supports.scoped_life_state
supports.scoped_reference_state
supports.quest_stage
supports.runtime_objectives
supports.cell_instance
```

Capabilities may help clients reject unsupported Contexts safely.

## 19. Type Versus Policy

Type should be descriptive.

Policy should define actual behavior.

Two Party Contexts may have different persistence or visibility rules.

This prevents Context Type from becoming a rigid collection of hidden assumptions.

## 20. Open Questions

- Should Global be a normal Context Type or a built-in layer?
- Should Narrative be a type or a policy attribute?
- Can one Context change type?
- Which policies are mutable?
- Can plugins define new Context Types?
- Which policy fields must be visible to clients?

## 21. Implementation Status

```text
Context types: Not implemented
Context policies: Not implemented
Party behavior: Implemented through legacy-specific systems
Plugin-defined Contexts: Not implemented
```
