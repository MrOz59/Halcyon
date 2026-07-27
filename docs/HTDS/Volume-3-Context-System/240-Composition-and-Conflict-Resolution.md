# HTDS-240 — Composition and Conflict Resolution

| Field | Value |
| --- | --- |
| Document ID | HTDS-240 |
| Title | Composition and Conflict Resolution |
| Version | 0.1.0 |
| Status | Draft Skeleton |
| Maturity | Conceptual / High-Risk Design Area |

## 1. Purpose

This document outlines how state from several active Contexts may produce one effective world view.

This is one of Halcyon's highest-risk architectural problems.

## 2. Composition

A Player may have several relevant Contexts:

```text
Global
Personal
Party
Narrative Instance
Public Event
```

Composition determines which state is visible when more than one Context provides data.

## 3. State Layers

Possible layers:

```text
Base Game Data
Global Context
Personal Context
Party Context
Active Narrative Instance
Temporary Session Override
```

This order is illustrative, not accepted precedence.

## 4. Component-Level Resolution

Precedence may need to be component-specific.

Example:

```text
Player visibility:
Global

Quest Actor life state:
Narrative Instance

Party objective:
Party Context

Public event marker:
Public Event Context
```

One universal precedence list may be insufficient.

## 5. Conflict Types

### Value conflict

Two Contexts provide different values for one component.

### Ownership conflict

Two Contexts claim authority over the same Entity.

### Lifecycle conflict

One Context destroys an Entity required by another.

### Quest conflict

Branches or stages are incompatible.

### Visibility conflict

One Context requires an Entity visible while another requires it hidden.

### Command conflict

Two Contexts issue incompatible local client commands.

## 6. Conflict Detection

Detection may use:

- same Entity and component key;
- quest profile metadata;
- branch identifiers;
- exclusivity flags;
- ownership claims;
- Context policy;
- runtime validation.

## 7. Resolution Strategies

Potential strategies:

- fixed precedence;
- reject Context activation;
- isolate one component;
- clone or variant state;
- create Narrative Instance;
- isolate full Entity;
- isolate cell;
- require manual choice;
- suspend one Context.

Automatic merge should be avoided when semantics are unknown.

## 8. Entity Variant

One promising model:

```text
Base Entity
├── Global components
├── Context A overrides
└── Context B overrides
```

The effective Entity is resolved per recipient.

## 9. Quest Conflict

Quest stages cannot be compared only numerically.

Example:

```text
Stage 50: kill target
Stage 60: spare target
```

A higher number does not imply a compatible later state.

Quest profiles or runtime analysis may be required.

## 10. Party and Personal Conflict

When joining a Party:

- Personal state remains preserved;
- Party state may become temporarily effective;
- unsafe differences are not merged automatically;
- the Player may participate as guest;
- progress commit occurs only through explicit policy.

## 11. Full Narrative Instance

A full Narrative Instance may be necessary when:

- actors participate in incompatible scenes;
- AI packages conflict;
- several References diverge;
- cell scripts assume one global state;
- Entity-level phasing becomes unsafe.

## 12. Merge Categories

Potential classifications:

- safely identical;
- safely fast-forwardable;
- compatible but requires replay;
- guest-only compatible;
- branch incompatible;
- unknown;
- unsupported.

Unknown should be treated conservatively.

## 13. Resolution Diagnostics

The server should explain:

- conflicting Context IDs;
- Entity or quest identity;
- components involved;
- selected policy;
- effective result;
- whether state was isolated or rejected.

## 14. Initial Prototype

The first prototype should avoid general composition.

Each Player has:

- Global Context;
- one Personal Context.

Only one component conflicts:

- Actor Life State.

Personal Context overrides base state for that component.

## 15. Open Questions

- Is composition calculated on mutation, subscription, or serialization?
- How are several active Narrative Instances ordered?
- Can plugins define conflict resolvers?
- Which resolver decisions must persist?
- Can client Skyrim safely switch variants while a cell is loaded?
- How are scenes and aliases handled?
- When is full cell isolation unavoidable?

## 16. Implementation Status

```text
Effective-state resolver: Not implemented
Conflict detection: Not implemented
Quest profiles: Not implemented
Entity variants: Not implemented
Narrative Instance resolution: Not implemented
```
